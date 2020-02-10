#include <windows.h>
#include <vector>

namespace polyfill
{
    typedef BOOL WINAPI GetContentRects_t(HWND hwnd, _Inout_ UINT* count, _Out_writes_opt_(*count) RECT* pContentRects);

    namespace details
    {
        struct content_rects
        {
            unsigned int count;
            std::vector<RECT> rects;

            content_rects()
            {
                count = 0;

                // Optimize for two screens.
                rects.reserve(2);
            }
        };

        BOOL CALLBACK CountWindowsCallback(const HMONITOR monitor, const HDC dc, const LPRECT rect, LPARAM param)
        {
            auto rects = (content_rects*)param;

            ++rects->count;
            rects->rects.push_back(*rect);

            return TRUE;
        }
    }

    BOOL WINAPI GetContentRects(HWND hwnd, _Inout_ UINT* count, _Out_writes_opt_(*count) RECT* pContentRects)
    {
        // Must have valid count ptr, and cannot pass null array ptr unless count is 0.
        if ((count == nullptr) || (pContentRects == nullptr && *count != 0))
        {
            SetLastError(ERROR_INVALID_PARAMETER);
            return FALSE;
        }

        BOOL result{ FALSE };
        details::content_rects rects;

        auto hDc = GetDC(hwnd);
        EnumDisplayMonitors(hDc, nullptr, details::CountWindowsCallback, (LPARAM)&rects);
        ReleaseDC(hwnd, hDc);

        // Copy as many rects as we have room for; if there are more than
        // will fit we return FALSE with ERROR_MORE_DATA.
        for (unsigned int i = 0; i < min(*count, rects.count); ++i)
        {
            pContentRects[i] = rects.rects[i];
        }
        if (*count < rects.count)
        {
            SetLastError(ERROR_MORE_DATA);
            result = FALSE;
        }
        else
        {
            SetLastError(ERROR_SUCCESS);
            result = TRUE;
        }

        *count = rects.count;
        return result;
    }
}

BOOL WINAPI GetContentRects(HWND hwnd, _Inout_ UINT* count, _Out_writes_opt_(*count) RECT* pContentRects)
{
    static polyfill::GetContentRects_t* impl{ nullptr };

    // No need for synchronization since worst-case two threads write the exact
    // same value into the pointer. The answer can't change at runtime.
    if (impl == nullptr)
    {
        auto module = LoadLibraryA("user32.dll");
        if (module != 0)
        {
            impl = (polyfill::GetContentRects_t*)GetProcAddress(module, "GetContentRects");
        }

        if (impl == nullptr)
        {
            impl = polyfill::GetContentRects;
        }
    }

    return impl(hwnd, count, pContentRects);
}