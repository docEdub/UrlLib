#include "UrlRequest_Windows_Base.h"

#include <arcana/threading/task_conversions.h>
#include <winrt/Windows.ApplicationModel.h>
#include <winrt/Windows.Storage.Streams.h>
#include <robuffer.h>

namespace UrlLib
{
    using namespace winrt::Windows;

    namespace
    {
        winrt::hstring GetInstalledLocation()
        {
            return ApplicationModel::Package::Current().InstalledLocation().Path();
        }
    }

    class UrlRequest::Impl::WindowsImpl
    {
    public:
        static void Initialize()
        {
        }

        static void Uninitialize()
        {
        }

        WindowsImpl(Impl& impl)
            : m_impl{impl}
        {}

        gsl::span<const std::byte> ResponseBuffer() const
        {
            if (!m_impl.m_winrtResponseBuffer)
            {
                return {};
            }

            std::byte* bytes;
            auto bufferByteAccess = m_impl.m_winrtResponseBuffer.as<::Windows::Storage::Streams::IBufferByteAccess>();
            winrt::check_hresult(bufferByteAccess->Buffer(reinterpret_cast<byte**>(&bytes)));
            return {bytes, gsl::narrow_cast<std::size_t>(m_impl.m_winrtResponseBuffer.Length())};
        }

        arcana::task<void, std::exception_ptr> LoadFileAsync(const std::wstring& path)
        {
            return arcana::create_task<std::exception_ptr>(Storage::StorageFile::GetFileFromPathAsync(path))
                .then(arcana::inline_scheduler, m_impl.m_cancellationSource, [this](Storage::StorageFile file) {
                    switch (m_impl.m_responseType)
                    {
                        case UrlResponseType::String:
                        {
                            return arcana::create_task<std::exception_ptr>(Storage::FileIO::ReadTextAsync(file))
                                .then(arcana::inline_scheduler, m_impl.m_cancellationSource, [this](winrt::hstring text) {
                                    m_impl.m_responseString = winrt::to_string(text);
                                    m_impl.m_statusCode = UrlStatusCode::Ok;
                                });
                        }
                        case UrlResponseType::Buffer:
                        {
                            return arcana::create_task<std::exception_ptr>(Storage::FileIO::ReadBufferAsync(file))
                                .then(arcana::inline_scheduler, m_impl.m_cancellationSource, [this](Storage::Streams::IBuffer buffer) {
                                    m_impl.m_winrtResponseBuffer = std::move(buffer);
                                    m_impl.m_statusCode = UrlStatusCode::Ok;
                                });
                        }
                        default:
                        {
                            throw std::runtime_error{"Invalid response type"};
                        }
                    }
                });
        }

    private:
        Impl& m_impl;
    };
}

#include "UrlRequest_Windows_Shared.h"
