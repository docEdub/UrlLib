// Shared pimpl code (not an actual header)

namespace UrlLib
{
    void UrlRequest::Impl::Initialize()
    {
        WindowsImpl::Initialize();
    }

    void UrlRequest::Impl::Uninitialize()
    {
        WindowsImpl::Uninitialize();
    }

    UrlRequest::Impl::Impl()
        : m_windowsImpl{std::make_shared<WindowsImpl>(*this)}
    {
    }

    gsl::span<const std::byte> UrlRequest::Impl::ResponseBuffer() const
    {
        return m_windowsImpl->ResponseBuffer();
    }

    arcana::task<void, std::exception_ptr> UrlRequest::Impl::LoadFileAsync(const std::wstring& path)
    {
        return m_windowsImpl->LoadFileAsync(path);
    }
}

#include "UrlRequest_Shared.h"
