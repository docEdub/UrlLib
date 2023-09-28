// Shared pimpl code (not an actual header)

namespace UrlLib
{
    UrlRequest::Impl::Impl()
        : m_windowsImpl{std::make_shared<WindowsImpl>(*this)}
    {

    }

    gsl::span<const std::byte> UrlRequest::Impl::ResponseBuffer() const
    {
        if (m_uri.SchemeName() == L"app" || m_uri.SchemeName() == L"file")
        {
            return m_windowsImpl->ResponseBuffer();
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
        return m_windowsImpl->LoadFileAsync(path);
    }
}

#include "UrlRequest_Shared.h"
