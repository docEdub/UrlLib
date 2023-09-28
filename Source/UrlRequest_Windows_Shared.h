// Shared pimpl code (not an actual header)

namespace UrlLib
{
    //using namespace winrt::Windows;

    std::shared_ptr<UrlRequest::Impl::WindowsImpl> UrlRequest::Impl::WindowsImpl()
    {
        if (!m_windowsImpl)
        {
            m_windowsImpl = std::make_shared<WindowsImpl>(*this);
        }
        return m_windowsImpl;
    }

    gsl::span<const std::byte> UrlRequest::Impl::ResponseBuffer() const
    {
        if (m_uri.SchemeName() == L"app" || m_uri.SchemeName() == L"file")
        {
            return WindowsImpl()->ResponseBuffer();
        }
        else
        {
            std::byte* bytes{nullptr};
            auto bufferByteAccess = m_winrtResponseBuffer.as<::Windows::Storage::Streams::IBufferByteAccess>();
            winrt::check_hresult(bufferByteAccess->Buffer(reinterpret_cast<byte**>(&bytes)));
            return {bytes, gsl::narrow_cast<std::size_t>(m_winrtResponseBuffer.Length())};
        }
    }

    arcana::task<void, std::exception_ptr> UrlRequest::Impl::LoadFileAsync(const std::wstring& path)
    {
        return WindowsImpl()->LoadFileAsync(path);
    }
}

#include "UrlRequest_Shared.h"
