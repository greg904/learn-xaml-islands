﻿#include "pch.h"

using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Hosting;
using namespace winrt::Windows::UI::Xaml::Controls;
using namespace winrt::Windows::UI::Xaml::Media;

class non_copyable
{
public:
    non_copyable()
    {
    }

    non_copyable(const non_copyable&) = delete;
    non_copyable& operator=(const non_copyable&) = delete;
};

class window_class : public non_copyable
{
public:
    window_class(const WNDCLASSEX *attributes, HINSTANCE hinstance)
    {
        const auto ret = RegisterClassEx(attributes);
        if (ret == 0)
        {
            winrt::throw_last_error();
        }

        _atom = ret;
        _hinstance = hinstance;
    }

    window_class(window_class&& other) noexcept :
        _atom(std::move(other._atom)),
        _hinstance(std::move(other._hinstance))
    {
        other._atom = 0;
        other._hinstance = NULL;
    }

    window_class& operator=(window_class&& other) noexcept
    {
        _atom = std::move(other._atom);
        _hinstance = std::move(other._hinstance);

        other._atom = 0;
        other._hinstance = NULL;
    }

    ~window_class()
    {
        if (_atom != 0)
        {
            winrt::check_bool(UnregisterClass(to_param(), _hinstance));
        }
    }

    LPCTSTR to_param() const noexcept
    {
        return reinterpret_cast<LPCTSTR>(_atom);
    }

private:
    ATOM _atom;
    HINSTANCE _hinstance;
};

class win32_window : public non_copyable
{
public:
    win32_window(HWND handle) : _handle(handle)
    {
    }

    win32_window(const window_class& wnd_class, LPCTSTR name, DWORD style, DWORD style_ex, int x, int y, int width, int height, HINSTANCE hinstance, HWND parent_handle = NULL)
    {
        const auto ret = CreateWindowEx(style_ex, wnd_class.to_param(), name, style, x, y, width, height, parent_handle, NULL, hinstance, this);
        if (ret == NULL)
        {
            winrt::throw_last_error();
        }

        _handle = ret;
    }

    win32_window(win32_window&& other) noexcept :
        _handle(std::move(other._handle)),
        _window_proc(std::move(other._window_proc))
    {
        other._handle = NULL;
    }

    win32_window& operator=(win32_window&& other) noexcept
    {
        _handle = std::move(other._handle);

        other._handle = NULL;

        return *this;
    }

    ~win32_window()
    {
        if (_handle != NULL)
        {
            winrt::check_bool(DestroyWindow(_handle));
        }
    }

    void show(int cmd_show) const noexcept
    {
        ShowWindow(_handle, cmd_show);
    }

    void resize(int x, int y, int width, int height) const
    {
        winrt::check_bool(MoveWindow(_handle, x, y, width, height, FALSE));
    }

    void bring_on_top() const
    {
        winrt::check_bool(SetWindowPos(_handle, HWND_TOP, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE | SWP_NOACTIVATE | SWP_NOOWNERZORDER));
    }

    void update_frame() const
    {
        winrt::check_bool(SetWindowPos(_handle, 0, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOSIZE | SWP_NOMOVE | SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOOWNERZORDER));
    }

    SIZE get_size() const
    {
        RECT client_rect;
        winrt::check_bool(GetClientRect(_handle, &client_rect));

        return {
            client_rect.right - client_rect.left,
            client_rect.bottom - client_rect.top
        };
    }

    UINT get_dpi() const
    {
        const auto ret = GetDpiForWindow(_handle);
        if (ret == 0)
        {
            winrt::throw_last_error();
        }

        return ret;
    }

    float get_dpi_scale() const
    {
        return static_cast<float>(get_dpi()) / static_cast<float>(USER_DEFAULT_SCREEN_DPI);
    }

    void set_window_proc(std::function<LRESULT(_In_ UINT msg, _In_ WPARAM w, _In_ LPARAM l)> window_proc)
    {
        _window_proc = window_proc;
    }

    HWND get_handle() const noexcept
    {
        return _handle;
    }

    static LRESULT CALLBACK global_window_proc(_In_ HWND hwnd, _In_ UINT msg, _In_ WPARAM w, _In_ LPARAM l) noexcept
    {
        if (msg == WM_CREATE)
        {
            const auto owner = reinterpret_cast<LPCREATESTRUCT>(l)->lpCreateParams;
            SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(owner));
        }

        auto owner = reinterpret_cast<win32_window*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
        if (owner == nullptr || !owner->_window_proc)
        {
            return DefWindowProc(hwnd, msg, w, l);
        }

        return owner->_window_proc(msg, w, l);
    }

private:
    HWND _handle;
    std::function<LRESULT(_In_ UINT msg, _In_ WPARAM w, _In_ LPARAM l)> _window_proc;
};

class xaml_island_window
{
public:
    xaml_island_window(HINSTANCE hinstance) : _hinstance(hinstance)
    {
        if (!_top_wnd_class)
        {
            WNDCLASSEX wc = {};
            wc.cbSize = sizeof(wc);
            wc.hInstance = hinstance;
            wc.lpfnWndProc = win32_window::global_window_proc;
            wc.lpszClassName = L"xaml_island_top_window_class";
            wc.hCursor = _normal_cursor;

            _top_wnd_class = std::make_unique<window_class>(&wc, hinstance);
        }

        _top_window = std::make_unique<win32_window>(*_top_wnd_class, L"LearnXamlIslands", WS_OVERLAPPEDWINDOW, WS_EX_OVERLAPPEDWINDOW | WS_EX_NOREDIRECTIONBITMAP, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, hinstance);
        _top_window->set_window_proc(std::bind(&xaml_island_window::_top_window_proc, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

        auto xaml_source_native = _xaml_source.as<IDesktopWindowXamlSourceNative>();
        xaml_source_native->AttachToWindow(_top_window->get_handle());

        TextBlock title_block;
        title_block.Text(L"Hello, XAML Islands world!");
        title_block.VerticalAlignment(VerticalAlignment::Center);
        title_block.Padding({ 10.0, 0.0, 0.0, 0.0 });

        Grid title_bar_grid;
        title_bar_grid.Children().Append(title_block);
        title_bar_grid.Height(50.0);

        AcrylicBrush brush;
        brush.BackgroundSource(AcrylicBackgroundSource::HostBackdrop);
        brush.TintOpacity(0.3);
        brush.TintColor({ 0xFF, 0x00, 0x80, 0x00 });
        brush.FallbackColor({ 0xFF, 0x00, 0x80, 0x00 });
        title_bar_grid.Background(brush);

        TextBlock body_block;
        body_block.Text(L"Acrylic title bar!");

        Grid body_grid;
        body_grid.Children().Append(body_block);
        body_grid.Padding({ 10.0, 10.0, 10.0, 10.0 });

        Grid grid;

        grid.RowDefinitions().Clear();

        RowDefinition title_row;
        title_row.Height({ 0.0, GridUnitType::Auto });
        grid.RowDefinitions().Append(title_row);
        grid.Children().Append(title_bar_grid);
        Grid::SetRow(title_bar_grid, 0);

        RowDefinition body_row;
        body_row.Height({ 1.0, GridUnitType::Star });
        grid.RowDefinitions().Append(body_row);
        grid.Children().Append(body_grid);
        Grid::SetRow(body_grid, 1);

        _xaml_source.Content(grid);

        winrt::check_hresult(xaml_source_native->get_WindowHandle(&_island_window_handle));
        _reposition_island_window(true);
    }

    ~xaml_island_window()
    {
        _xaml_source.Close();
    }

    void show(int cmd_show)
    {
        _top_window->show(cmd_show);
    }

    void set_extend_title_bar_into_client_area(bool value)
    {
        if (_extend_title_bar_into_client_area != value)
        {
            _extend_title_bar_into_client_area = value;
            _top_window->update_frame();
        }
    }

    void set_drag_area(const std::vector<RECT>& client_rects)
    {
        auto drag_wnd_it = _drag_windows.cbegin();

        for (const auto& rect : client_rects)
        {
            int x = rect.left;
            int y = rect.top;
            int width = rect.right - rect.left;
            int height = rect.bottom - rect.top;

            if (drag_wnd_it == _drag_windows.cend())
            {
                if (!_drag_wnd_class)
                {
                    WNDCLASSEX wc = {};
                    wc.cbSize = sizeof(wc);
                    wc.hInstance = _hinstance;
                    wc.lpfnWndProc = win32_window::global_window_proc;
                    wc.lpszClassName = L"xaml_island_drag_window_class";

                    _drag_wnd_class = std::make_unique<window_class>(&wc, _hinstance);
                }

                // Add a new window if we don't have windows to reuse anymore.
                // Description of window styles:
                // - Use WS_CLIPSIBLING to clip the XAML Island window to make
                //   sure that our window is on top of it and receives all mouse
                //   input.
                // - WS_EX_LAYERED is required. If it is not present, then for
                //   some reason, the window will not receive any mouse input.
                // - WS_EX_NOREDIRECTIONBITMAP makes the window invisible (we
                //   could also set its opacity to 0 because it's a layered
                //   window but this is simpler).
                auto& wnd = _drag_windows.emplace_back(*_drag_wnd_class, L"", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS, WS_EX_LAYERED | WS_EX_NOREDIRECTIONBITMAP, x, y, width, height, _hinstance, _top_window->get_handle());
                wnd.set_window_proc(std::bind(&xaml_island_window::_drag_window_proc, this, std::ref(wnd), std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
                wnd.bring_on_top();

                // it will become invalid after the `emplace_back` so we reset it
                drag_wnd_it = _drag_windows.cend();
            }
            else
            {
                // recycle old window
                drag_wnd_it->resize(x, y, width, height);

                drag_wnd_it++;
            }
        }

        // remove now ununused windows
        _drag_windows.erase(drag_wnd_it, _drag_windows.cend());
    }

    void set_resize_cb(std::function<void(int new_width, int new_height)> cb)
    {
        _resize_cb = cb;
    }

    SIZE get_size() const
    {
        return _top_window->get_size();
    }

    float get_dpi_scale() const
    {
        return _top_window->get_dpi_scale();
    }

private:
    LRESULT _top_window_proc(_In_ UINT msg, _In_ WPARAM w, _In_ LPARAM l) noexcept
    {
        switch (msg)
        {
        case WM_PARENTNOTIFY:
            if (w == WM_LBUTTONDOWN)
            {
                POINT client_pt = { GET_X_LPARAM(l), GET_Y_LPARAM(l) };

                POINT screen_pt = client_pt;
                if (ClientToScreen(_top_window->get_handle(), &screen_pt))
                {
                    WPARAM cmd = -1;

                    LRESULT hit_test = SendMessage(_top_window->get_handle(), WM_NCHITTEST, 0, MAKELPARAM(screen_pt.x, screen_pt.y));
                    switch (hit_test)
                    {
                    case HTCAPTION:
                        cmd = SC_MOVE + hit_test;
                        break;
                    case HTTOP:
                        cmd = SC_SIZE + WMSZ_TOP;
                        break;
                    }

                    if (cmd != -1)
                    {
                        PostMessage(_top_window->get_handle(), WM_SYSCOMMAND, cmd, MAKELPARAM(client_pt.x, client_pt.y));
                    }
                }
            }
            break;
        case WM_SETCURSOR:
        {
            if (LOWORD(l) == HTCLIENT)
            {

                // Get the cursor position from the _last message_ and not from
                // `GetCursorPos` (which returns the cursor position _at the
                // moment_) because if we're lagging behind the cursor's position,
                // we still want to get the cursor position that was associated
                // with that message at the time it was sent to handle the message
                // correctly.
                const auto screen_pt_dword = GetMessagePos();
                POINT screen_pt = { GET_X_LPARAM(screen_pt_dword), GET_Y_LPARAM(screen_pt_dword) };

                LRESULT hit_test = SendMessage(_top_window->get_handle(), WM_NCHITTEST, 0, MAKELPARAM(screen_pt.x, screen_pt.y));
                if (hit_test == HTTOP)
                {
                    // We have to set the vertical resize cursor manually on
                    // the top resize handle because Windows thinks that the
                    // cursor is on the client area because it asked the asked
                    // the drag window with `WM_NCHITTEST` and it returned
                    // `HTCLIENT`.
                    // We don't want to modify the drag window's `WM_NCHITTEST`
                    // handling to return `HTTOP` because otherwise, the system
                    // would resize the drag window instead of the top level
                    // window!
                    SetCursor(_vertical_resize_cursor);
                    return TRUE;
                }
                else
                {
                    // reset cursor
                    SetCursor(_normal_cursor);
                    return TRUE;
                }
            }
            break;
        }
        case WM_SIZE:
            _reposition_island_window();
            
            if (_resize_cb)
            {
                _resize_cb(LOWORD(l), HIWORD(l));
            }

            break;
        case WM_DPICHANGED:
        {
            RECT *suggested_rect = reinterpret_cast<RECT*>(l);
            int x = suggested_rect->left;
            int y = suggested_rect->top;
            int width = suggested_rect->right - suggested_rect->left;
            int height = suggested_rect->bottom - suggested_rect->top;
            _top_window->resize(x, y, width, height);
            return 0;
        }
        case WM_NCHITTEST:
        {
            POINT pt = { GET_X_LPARAM(l), GET_Y_LPARAM(l) };

            // This will handle the left, right and bottom parts of the frame because
            // we didn't change them.
            const auto originalRet = DefWindowProc(_top_window->get_handle(), WM_NCHITTEST, w, l);
            if (originalRet != HTCLIENT)
            {
                return originalRet;
            }

            RECT window_rect;
            if (!GetWindowRect(_top_window->get_handle(), &window_rect))
            {
                return originalRet;
            }

            const auto dpi = _top_window->get_dpi();
            const auto top_resize_handle_height =
                GetSystemMetricsForDpi(SM_CXPADDEDBORDER, dpi) + // there isn't a SM_CYPADDEDBORDER for the Y axis
                GetSystemMetricsForDpi(SM_CYSIZEFRAME, dpi);
            if (pt.y < window_rect.top + top_resize_handle_height)
            {
                return HTTOP;
            }

            for (const auto& wnd : _drag_windows)
            {
                RECT drag_window_rect;
                if (!GetWindowRect(wnd.get_handle(), &drag_window_rect))
                {
                    continue;
                }

                if (pt.x >= drag_window_rect.left && pt.x < drag_window_rect.right &&
                    pt.y >= drag_window_rect.top && pt.y < drag_window_rect.bottom)
                {
                    return HTCAPTION;
                }
            }

            return HTCLIENT;
        }
        case WM_NCCALCSIZE:
            if (_extend_title_bar_into_client_area)
            {
                if (w == TRUE)
                {
                    auto params = reinterpret_cast<NCCALCSIZE_PARAMS*>(l);

                    const auto windowTop = params->rgrc[0].top;

                    // apply the default non-client frame
                    const auto ret = DefWindowProc(_top_window->get_handle(), WM_NCCALCSIZE, w, l);

                    // remove the added non-client frame at the top
                    params->rgrc[0].top = windowTop;

                    // TODO: We should probably set the 2 other RECTS in
                    //  `params->rgrc` (see the doc for `WM_NCCALCSIZE`).

                    return ret;
                }
                else if (w == FALSE)
                {
                    auto rect = reinterpret_cast<RECT*>(l);

                    const auto windowTop = rect->top;

                    // apply the default non-client frame
                    const auto ret = DefWindowProc(_top_window->get_handle(), WM_NCCALCSIZE, w, l);

                    // remove the added non-client frame at the top
                    rect->top = windowTop;

                    return ret;
                }
            }
        case WM_CLOSE:
            PostQuitMessage(0);
            return 0;
        }

        return DefWindowProc(_top_window->get_handle(), msg, w, l);
    }

    LRESULT _drag_window_proc(win32_window& wnd, _In_ UINT msg, _In_ WPARAM w, _In_ LPARAM l) noexcept
    {
        switch (msg)
        {
        }

        return DefWindowProc(wnd.get_handle(), msg, w, l);
    }

    void _reposition_island_window(bool show = false) const
    {
        RECT client_rc;
        winrt::check_bool(GetClientRect(_top_window->get_handle(), &client_rc));

        int x = client_rc.left;
        int y = client_rc.top;
        int width = client_rc.right - client_rc.left;
        int height = client_rc.bottom - client_rc.top;

        if (show)
        {
            winrt::check_bool(SetWindowPos(_island_window_handle, HWND_BOTTOM, x, y, width, height, SWP_SHOWWINDOW | SWP_NOACTIVATE | SWP_NOOWNERZORDER));
        }
        else
        {
            winrt::check_bool(MoveWindow(_island_window_handle, x, y, width, height, FALSE));
        }
    }

    static HCURSOR _load_cursor(WORD type)
    {
        const auto ret = reinterpret_cast<HCURSOR>(LoadImage(NULL, MAKEINTRESOURCE(type), IMAGE_CURSOR, 0, 0, LR_SHARED | LR_DEFAULTSIZE));
        if (ret == NULL)
        {
            winrt::throw_last_error();
        }

        return ret;
    }

    static std::unique_ptr<window_class> _top_wnd_class;
    static std::unique_ptr<window_class> _drag_wnd_class;
    static HCURSOR _normal_cursor;
    static HCURSOR _vertical_resize_cursor;

    HINSTANCE _hinstance;
    bool _extend_title_bar_into_client_area;
    std::unique_ptr<win32_window> _top_window;
    std::vector<win32_window> _drag_windows;
    HWND _island_window_handle;
    DesktopWindowXamlSource _xaml_source;
    std::function<void(int new_width, int new_height)> _resize_cb;
};

std::unique_ptr<window_class> xaml_island_window::_top_wnd_class;
std::unique_ptr<window_class> xaml_island_window::_drag_wnd_class;
HCURSOR xaml_island_window::_normal_cursor = xaml_island_window::_load_cursor(OCR_NORMAL);
HCURSOR xaml_island_window::_vertical_resize_cursor = xaml_island_window::_load_cursor(OCR_SIZENS);

int run_msg_loop()
{
    MSG msg;

    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // WM_QUIT's wParam is the exit code
    return static_cast<int>(msg.wParam);
}

int WINAPI wWinMain(_In_ HINSTANCE hinstance, _In_opt_ HINSTANCE, _In_ LPWSTR, _In_ int cmd_show)
{
    winrt::init_apartment(winrt::apartment_type::single_threaded);

    xaml_island_window wnd(hinstance);
    wnd.set_extend_title_bar_into_client_area(true);

    const auto size = wnd.get_size();
    const auto dpi = wnd.get_dpi_scale();
    wnd.set_drag_area({ {0, 0, static_cast<LONG>(size.cx * dpi), static_cast<LONG>(50 * dpi)} });

    wnd.set_resize_cb([&](int new_width, int new_height)
        {
            const auto dpi = wnd.get_dpi_scale();
            wnd.set_drag_area({ {0, 0, static_cast<LONG>(new_width * dpi), static_cast<LONG>(50 * dpi)} });
        });

    wnd.show(cmd_show);

    return run_msg_loop();
}
