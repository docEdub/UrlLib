#include "UrlRequest_Windows_Base.h"

#include <fstream>
#include <sstream>

namespace UrlLib
{
    namespace
    {
        winrt::hstring GetInstalledLocation()
        {
            WCHAR modulePath[4096];
            DWORD result{::GetModuleFileNameW(nullptr, modulePath, ARRAYSIZE(modulePath))};
            winrt::check_bool(result != 0 && result != std::size(modulePath));
            winrt::check_hresult(PathCchRemoveFileSpec(modulePath, ARRAYSIZE(modulePath)));
            return modulePath;
        }
    }

    class UrlRequest::Impl::WindowsImpl
    {
    public:
        WindowsImpl(Impl& impl)
            : m_impl{impl}
        {}

    private:

        gsl::span<const std::byte> ResponseBuffer() const
        {
            return {(std::byte*)m_fileResponseBuffer.data(), gsl::narrow_cast<std::size_t>(m_fileResponseBuffer.size())};
        }

        arcana::task<void, std::exception_ptr> LoadFileAsync(const std::wstring& path)
        {
            switch (m_impl.m_responseType)
            {
                case UrlResponseType::String:
                {
                    return arcana::make_task(arcana::threadpool_scheduler, m_impl.m_cancellationSource, [this, path] {
                        std::ifstream file(path);
                        if (!file.good())
                        {
                            std::stringstream msg;
                            msg << "Failed to read file " << path.c_str();
                            throw std::runtime_error{msg.str()};
                        }

                        std::stringstream ss;
                        ss << file.rdbuf();

                        m_impl.m_responseString = ss.str();
                        m_impl.m_statusCode = UrlStatusCode::Ok;
                    });
                }
                case UrlResponseType::Buffer:
                {
                    return arcana::make_task(arcana::threadpool_scheduler, m_impl.m_cancellationSource, [this, path] {
                        std::ifstream file(path, std::ios::binary);
                        if (!file.good())
                        {
                            std::stringstream msg;
                            msg << "Failed to read file " << path.c_str();
                            throw std::runtime_error{msg.str()};
                        }

                        m_fileResponseBuffer.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
                        m_impl.m_statusCode = UrlStatusCode::Ok;
                    });
                }
                default:
                {
                    throw std::runtime_error{"Invalid response type"};
                }
            }
        }

        Impl& m_impl;
        std::vector<char> m_fileResponseBuffer;
    };
}
