// license:BSD-3-Clause
// copyright-holders: Brad Hughes, Aaron Giles
/*
 * monitor_win32.cpp
 *
 */

#include "modules/osdmodule.h"
#include "monitor_module.h"

#if defined(OSD_WINDOWS)

// standard windows headers
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef interface
#undef min
#undef max

#include "strconv.h"
#include "windows/video.h"
#include "window.h"
#include "monitor_common.h"

class win32_monitor_module;

class win32_monitor_info : public osd_monitor_info
{
private:
	MONITORINFOEX    m_info;

public:
	win32_monitor_info(monitor_module& module, const HMONITOR handle, const char* monitor_device, float aspect)
		: osd_monitor_info(module, std::intptr_t(handle), monitor_device, aspect)
	{
		win32_monitor_info::refresh();
	}

	void refresh() override
	{
		BOOL result;

		// fetch the latest info about the monitor
		m_info.cbSize = sizeof(m_info);
		result = GetMonitorInfo(reinterpret_cast<HMONITOR>(oshandle()), static_cast<LPMONITORINFO>(&m_info));
		assert(result);

		m_name = osd::text::from_tstring(m_info.szDevice);

		m_pos_size = RECT_to_osd_rect(m_info.rcMonitor);
		m_usuable_pos_size = RECT_to_osd_rect(m_info.rcWork);
		m_is_primary = ((m_info.dwFlags & MONITORINFOF_PRIMARY) != 0);
		(void)result; // to silence gcc 4.6
	}
};

class win32_monitor_module : public monitor_module_base
{
public:
	win32_monitor_module()
		: monitor_module_base(OSD_MONITOR_PROVIDER, "win32")
	{
	}

	std::shared_ptr<osd_monitor_info> monitor_from_rect(const osd_rect& rect) override
	{
		if (!m_initialized)
			return nullptr;

		RECT p;
		p.top = rect.top();
		p.left = rect.left();
		p.bottom = rect.bottom();
		p.right = rect.right();

		auto nearest = monitor_from_handle(reinterpret_cast<std::uintptr_t>(MonitorFromRect(&p, MONITOR_DEFAULTTONEAREST)));
		assert(nearest != nullptr);
		return nearest;
	}

	std::shared_ptr<osd_monitor_info> monitor_from_window(const osd_window& window) override
	{
		if (!m_initialized)
			return nullptr;

		auto nearest = monitor_from_handle(reinterpret_cast<std::uintptr_t>(MonitorFromWindow(window.platform_window<HWND>(), MONITOR_DEFAULTTONEAREST)));
		assert(nearest != nullptr);
		return nearest;
	}

protected:
	int init_internal(const osd_options& options) override
	{
		// make a list of monitors
		EnumDisplayMonitors(nullptr, nullptr, monitor_enum_callback, reinterpret_cast<std::intptr_t>(this));

		// if we're verbose, print the list of monitors
		{
			for (auto monitor : list())
			{
				osd_printf_verbose("Video: Monitor %I64u = \"%s\" %s\n", monitor->oshandle(), monitor->devicename().c_str(), monitor->is_primary() ? "(primary)" : "");
			}
		}

		return 0;
	}

private:
	static BOOL CALLBACK monitor_enum_callback(HMONITOR handle, HDC dc, LPRECT rect, LPARAM data)
	{
		win32_monitor_module* self = reinterpret_cast<win32_monitor_module*>(data);
		MONITORINFOEX info;
		BOOL result;

		// get the monitor info
		info.cbSize = sizeof(info);
		result = GetMonitorInfo(handle, static_cast<LPMONITORINFO>(&info));
		assert(result);
		(void)result; // to silence gcc 4.6

					  // guess the aspect ratio assuming square pixels
		float aspect = static_cast<float>(info.rcMonitor.right - info.rcMonitor.left) / static_cast<float>(info.rcMonitor.bottom - info.rcMonitor.top);

		// allocate a new monitor info
		auto temp = osd::text::from_tstring(info.szDevice);

		// copy in the data
		auto monitor = std::make_shared<win32_monitor_info>(*self, handle, temp.c_str(), aspect);

		// hook us into the list
		self->add_monitor(monitor);

		// enumerate all the available monitors so to list their names in verbose mode
		return TRUE;
	}
};

#else
MODULE_NOT_SUPPORTED(win32_monitor_module, OSD_MONITOR_PROVIDER, "win32")
#endif

MODULE_DEFINITION(MONITOR_WIN32, win32_monitor_module)