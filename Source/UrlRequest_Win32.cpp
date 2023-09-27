#include "UrlRequest_Base.h"

#include <Unknwn.h>
#include <PathCch.h>
#include <arcana/threading/task_conversions.h>
#include <arcana/threading/task_schedulers.h>
#include <robuffer.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.Web.Http.h>
#include <winrt/Windows.ApplicationModel.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Web.Http.Headers.h>

#include <fstream>
#include <sstream>

namespace UrlLib
{
    using namespace winrt::Windows;

    namespace
    {
        Web::Http::HttpMethod ConvertHttpMethod(UrlMethod method)
        {
            switch (method)
            {
                case UrlMethod::Get:
                    return Web::Http::HttpMethod::Get();
                case UrlMethod::Post:
                    return Web::Http::HttpMethod::Post();
                default:
                    throw std::runtime_error("Unsupported method");
            }
        }

        winrt::hstring GetInstalledLocation()
        {
#ifdef WIN32
            WCHAR modulePath[4096];
            DWORD result{::GetModuleFileNameW(nullptr, modulePath, ARRAYSIZE(modulePath))};
            winrt::check_bool(result != 0 && result != std::size(modulePath));
            winrt::check_hresult(PathCchRemoveFileSpec(modulePath, ARRAYSIZE(modulePath)));
            return modulePath;
#else
            return ApplicationModel::Package::Current().InstalledLocation().Path;
#endif
        }

        std::wstring GetLocalPath(Foundation::Uri url)
        {
            std::wstring path{std::wstring_view{Foundation::Uri::UnescapeComponent(url.Path())}.substr(1)};
            std::replace(path.begin(), path.end(), '/', '\\');
            return std::move(path);
        }

        std::wstring ResolveSymlink(const std::wstring& path)
        {
            std::wstring resolvedPath{path};

            while (std::filesystem::is_symlink(resolvedPath))
            {
                resolvedPath = std::filesystem::read_symlink(resolvedPath).wstring();
            }

            std::replace(resolvedPath.begin(), resolvedPath.end(), L'/', L'\\');

            return std::move(resolvedPath);
        }
    }

    class UrlRequest::Impl : public ImplBase
    {
    public:
        void Open(UrlMethod method, const std::string& url)
        {
            m_method = method;
            m_uri = Foundation::Uri{winrt::to_hstring(url)};
        }

        arcana::task<void, std::exception_ptr> SendAsync()
        {
            try
            {
                if (m_uri.SchemeName() == L"app" || m_uri.SchemeName() == L"file")
                {
                    auto path = GetLocalPath(m_uri);
                    if (m_uri.SchemeName() == L"app")
                    {
                        path = std::wstring(GetInstalledLocation()) + L'\\' + path;
                    }

                    //path = ResolveSymlink(path);

                    //return arcana::create_task<std::exception_ptr>(Storage::StorageFile::GetFileFromPathAsync(path))
                    //    .then(arcana::inline_scheduler, m_cancellationSource, [this](Storage::StorageFile file) {
                    //        return LoadFileAsync(file);
                    //    });
                    return LoadFileAsync(path);
                }
                else
                {
                    Web::Http::HttpRequestMessage requestMessage;
                    requestMessage.RequestUri(m_uri);
                    requestMessage.Method(ConvertHttpMethod(m_method));

                    std::string contentType;

                    for (auto request : m_requestHeaders)
                    {
                        // content type needs to be set separately
                        if (request.first == "Content-Type")
                        {
                            contentType = request.second;
                        }
                        else
                        {
                            requestMessage.Headers().Append(winrt::to_hstring(request.first), winrt::to_hstring(request.second));
                        }
                    }

                    m_requestHeaders.clear();

                    // check the method
                    if (m_method == UrlMethod::Post)
                    {
                        // if post, set the content type
                        requestMessage.Content(Web::Http::HttpStringContent(
                            winrt::to_hstring(m_requestBody),
                            winrt::Windows::Storage::Streams::UnicodeEncoding::Utf8,
                            winrt::to_hstring(contentType))
                        );
                    }

                    Web::Http::HttpClient client;
                    return arcana::create_task<std::exception_ptr>(client.SendRequestAsync(requestMessage))
                        .then(arcana::inline_scheduler, m_cancellationSource, [this](Web::Http::HttpResponseMessage responseMessage)
                        {
                            m_statusCode = static_cast<UrlStatusCode>(responseMessage.StatusCode());
                            if (!responseMessage.IsSuccessStatusCode())
                            {
                                return arcana::task_from_result<std::exception_ptr>();
                            }

                            m_responseUrl = winrt::to_string(responseMessage.RequestMessage().RequestUri().RawUri());

                            for (auto&& iter : responseMessage.Headers())
                            {
                                m_headers.insert(std::make_pair(winrt::to_string(iter.Key()), winrt::to_string(iter.Value())));
                            }
                            // process the content type response header
                            std::string contentTypeValue = winrt::to_string(responseMessage.Content().Headers().ContentType().ToString());
                            std::string contentTypeKey = "content-type";
                            m_headers.insert(std::make_pair(contentTypeKey, contentTypeValue));

                            switch (m_responseType)
                            {
                                case UrlResponseType::String:
                                {
                                    return arcana::create_task<std::exception_ptr>(responseMessage.Content().ReadAsStringAsync())
                                        .then(arcana::inline_scheduler, m_cancellationSource, [this](winrt::hstring string)
                                        {
                                            m_responseString = winrt::to_string(string);
                                            m_statusCode = UrlStatusCode::Ok;
                                        });
                                }
                                case UrlResponseType::Buffer:
                                {
                                    return arcana::create_task<std::exception_ptr>(responseMessage.Content().ReadAsBufferAsync())
                                        .then(arcana::inline_scheduler, m_cancellationSource, [this](Storage::Streams::IBuffer buffer)
                                        {
                                            SetResponseBuffer(buffer);
                                            m_statusCode = UrlStatusCode::Ok;
                                        });
                                }
                                default:
                                {
                                    throw std::runtime_error{"Invalid response type"};
                                }
                            }
                        });
                }
            }
            catch (winrt::hresult_error)
            {
                // Catch WinRT exceptions, but retain the default status code of 0 to indicate a client side error.
                return arcana::task_from_result<std::exception_ptr>();
            }
        }

        gsl::span<const std::byte> ResponseBuffer() const
        {
            return m_responseBuffer;
        }

        void SetResponseBuffer(Storage::Streams::IBuffer responseBuffer)
        {
            std::byte* bytes{nullptr};
            auto bufferByteAccess = responseBuffer.as<::Windows::Storage::Streams::IBufferByteAccess>();
            winrt::check_hresult(bufferByteAccess->Buffer(reinterpret_cast<byte**>(&bytes)));
            m_responseBuffer = {bytes, gsl::narrow_cast<std::size_t>(responseBuffer.Length())};
        }

        void SetResponseBuffer(std::vector<char>& responseBuffer)
        {
            m_responseBuffer = {(std::byte*)responseBuffer.data(), gsl::narrow_cast<std::size_t>(responseBuffer.size())};
        }

    private:
        arcana::task<void, std::exception_ptr> LoadFileAsync(const std::wstring& path)
        {
            switch (m_responseType)
            {
                case UrlResponseType::String:
                {
                    return arcana::make_task(arcana::threadpool_scheduler, m_cancellationSource, [this, path] {
                        std::ifstream file(path);
                        if (!file.good())
                        {
                            std::stringstream msg;
                            msg << "Failed to read file " << path.c_str();
                            throw std::runtime_error{msg.str()};
                        }

                        std::stringstream ss;
                        ss << file.rdbuf();

                        m_responseString = ss.str();
                        m_statusCode = UrlStatusCode::Ok;
                    });
                }
                case UrlResponseType::Buffer:
                {
                    return arcana::make_task(arcana::threadpool_scheduler, m_cancellationSource, [this, path] {
                        std::ifstream file(path, std::ios::binary);
                        if (!file.good())
                        {
                            std::stringstream msg;
                            msg << "Failed to read file " << path.c_str();
                            throw std::runtime_error{msg.str()};
                        }

                        m_fileResponseBuffer.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());

                        SetResponseBuffer(m_fileResponseBuffer);
                        m_statusCode = UrlStatusCode::Ok;
                    });
                }
                default:
                {
                    throw std::runtime_error{"Invalid response type"};
                }
            }
        }

        Foundation::Uri m_uri{nullptr};
        Storage::Streams::IBuffer m_httpResponseBuffer{};
        std::vector<char> m_fileResponseBuffer;
        gsl::span<std::byte> m_responseBuffer;
    };
}

#include "UrlRequest_Shared.h"
