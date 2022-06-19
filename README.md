# learn-xaml-islands

This is a repository where I tried making a XAML component into a draggable
title bar in a Win32 application that uses XAML Islands. The goal was in
particular to show a title bar with an [acrylic background](https://docs.microsoft.com/en-us/windows/apps/design/style/acrylic),
as in many UWP applications.

With a UWP application, it is easy to do that because the XAML framework
provides a [simple API](https://docs.microsoft.com/en-us/uwp/api/windows.applicationmodel.core.coreapplicationviewtitlebar.extendviewintotitlebar?view=winrt-22621)
to draw into the existing title bar with XAML. Unfortunately that API doesn't
work with XAML Islands.

Note that this was done before WinUI3 was released and the situation has
probably changed in the meanwhile.
