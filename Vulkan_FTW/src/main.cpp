#include <windows.h>
#include <assert.h>

#include <vulkan/vulkan.h>

#include <iostream>

#include <cstdlib>

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))

#define APP_NAME "Vulkan FTW"
#define ENGINE_NAME "YOLORenderSQUAD"


static HINSTANCE	win_instance;
static LPCSTR		win_class_name = "vulkan_render_window";
static LPCSTR		win_app_name = APP_NAME;
static LONG			win_width = 800;
static LONG			win_height = 640;
static HWND			window_handle;

static bool			vk_validate = true; // Tells the application if it should load Vulkan's validation layers.
static char*		vk_instance_layers[] = {
	"VK_LAYER_LUNARG_standard_validation"
};
static char*		vk_instance_extensions[] = {
	"VK_KHR_surface",
	"VK_KHR_win32_surface",
	"VK_EXT_debug_report"
};
static char*		vk_device_extensions[] = {
	"VK_KHR_swapchain"
};

static VkInstance				vk_instance;

static VkPhysicalDevice			vk_gpu;
static uint32_t					vk_queue_count = 0;
static VkQueueFamilyProperties*	vk_queue_props = nullptr;



static void create_window();
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
int main(int, char**);
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int);

static void vk_init();
static void vk_shutdown();


static void
create_window()
{
	WNDCLASSEX win_class;

	win_class.cbSize = sizeof(WNDCLASSEX);
	win_class.style = CS_HREDRAW | CS_VREDRAW;
	win_class.lpfnWndProc = WndProc;
	win_class.cbClsExtra = 0;
	win_class.cbWndExtra = 0;
	win_class.hInstance = win_instance;
	win_class.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	win_class.hCursor = LoadCursor(NULL, IDC_ARROW);
	win_class.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
	win_class.lpszMenuName = NULL;
	win_class.lpszClassName = win_class_name;
	win_class.hIconSm = LoadIcon(NULL, IDI_WINLOGO);

	// NOTE: This returns a bool that can be tested if the call fails.
	assert(RegisterClassEx(&win_class));

	RECT win_rect = { 0, 0, win_width, win_height };
	AdjustWindowRect(&win_rect, WS_OVERLAPPEDWINDOW, FALSE);
	window_handle = CreateWindowEx(0, 
								   win_class_name, 
								   win_app_name, 
								   WS_OVERLAPPEDWINDOW | WS_VISIBLE | WS_SYSMENU,
								   CW_USEDEFAULT, CW_USEDEFAULT,
								   win_width, win_height,
								   NULL, NULL,
								   win_instance,
								   NULL);

	// NOTE: We could check if the window was created here.
	assert(window_handle);
}


LRESULT CALLBACK
WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_CLOSE:
		PostQuitMessage(0);
		break;
	case WM_PAINT:
		// DRAW HERE
		break;
	case WM_SIZE:
		// RESIZE HERE
		break;
	default: break;
	}
	return (DefWindowProc(hWnd, uMsg, wParam, lParam));
}


int
main(int, char**)
{
	return WinMain(GetModuleHandle(nullptr), nullptr, nullptr, SW_SHOW);
}


int WINAPI 
WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int)
{
	// TODO: Add an argument parser in order to
	//		 - enable/disable validation layers
	MSG		msg;
	bool	run = true;

	win_instance = hInstance;

	create_window();
	vk_init();

	while (run)
	{
		PeekMessage(&msg, NULL, 0, 0, PM_REMOVE);
		if (msg.message == WM_QUIT)
			run = false;
		else
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		RedrawWindow(window_handle, NULL, NULL, RDW_INTERNALPAINT);
	}

	vk_shutdown();

	return (int)msg.wParam;
}


static void
vk_init()
{
	VkResult	error;

	// NOTE: If any requested layer or extension is not supported by the driver,
	//		 an assert is raised. 

	// VALIDATION LAYERS CHECK
	if (vk_validate)
	{
		uint32_t	instance_layer_count = 0;

		error = vkEnumerateInstanceLayerProperties(&instance_layer_count, nullptr);
		assert(!error);

		if (instance_layer_count > 0)
		{
			VkLayerProperties* instance_layers = new VkLayerProperties[instance_layer_count];

			error = vkEnumerateInstanceLayerProperties(&instance_layer_count, instance_layers);
			assert(!error);

			// Make sure that every validation layer that we want is supported by the ICD.
			bool validation_ok = true;
			{
				for (uint32_t expected = 0; expected < ARRAY_SIZE(vk_instance_layers) && validation_ok; ++expected) 
				{
					char*	expected_layer = vk_instance_layers[expected];
					bool	expected_ok = false;
					for (uint32_t available = 0; available < instance_layer_count && !expected_ok; ++available)
					{
						expected_ok = !strcmp(expected_layer, instance_layers[available].layerName);
					}
					validation_ok &= expected_ok;
				}
			}
			assert(validation_ok);

			delete[] instance_layers;
		}
	}

	// EXTENSIONS CHECK
	{
		uint32_t	instance_extension_count = 0;

		error = vkEnumerateInstanceExtensionProperties(nullptr, &instance_extension_count, nullptr);
		assert(!error);

		if (instance_extension_count > 0)
		{
			VkExtensionProperties* instance_extensions = new VkExtensionProperties[instance_extension_count];

			error = vkEnumerateInstanceExtensionProperties(nullptr, &instance_extension_count, instance_extensions);
			assert(!error);

			bool extensions_ok = true;
			{
				for (uint32_t expected = 0; expected < ARRAY_SIZE(vk_instance_extensions) && extensions_ok; ++expected)
				{
					char*	expected_extension = vk_instance_extensions[expected];
					bool	expected_ok = false;
					for (uint32_t available = 0; available < instance_extension_count && !expected_ok; ++available)
					{
						expected_ok = !strcmp(expected_extension, instance_extensions[available].extensionName);
					}
					extensions_ok &= expected_ok;
				}
			}
			assert(extensions_ok);

			delete[] instance_extensions;
		}
	}

	// INSTANCE
	{
		VkInstanceCreateInfo	instance_info;
		VkApplicationInfo		app_info;

		// VkApplicationInfo
		{
			app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
			app_info.pNext = nullptr;
			app_info.pApplicationName = APP_NAME;
			app_info.applicationVersion = 1;
			app_info.pEngineName = ENGINE_NAME;
			app_info.engineVersion = 1;
			app_info.apiVersion = VK_API_VERSION_1_0;
		}

		// VkInstanceCreateInfo
		{
			instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
			instance_info.pNext = nullptr;
			instance_info.pApplicationInfo = &app_info;
			instance_info.enabledLayerCount = (uint32_t)ARRAY_SIZE(vk_instance_layers);
			instance_info.ppEnabledLayerNames = vk_instance_layers;
			instance_info.enabledExtensionCount = (uint32_t)ARRAY_SIZE(vk_instance_extensions);
			instance_info.ppEnabledExtensionNames = vk_instance_extensions;
		}

		error = vkCreateInstance(&instance_info, nullptr, &vk_instance);
		assert(!error);
	}

	// PHYSICAL DEVICE
	{
		uint32_t	gpu_count = 0;

		error = vkEnumeratePhysicalDevices(vk_instance, &gpu_count, nullptr);
		assert(!error && gpu_count);

		VkPhysicalDevice* instance_physical_devices = new VkPhysicalDevice[gpu_count];
		error = vkEnumeratePhysicalDevices(vk_instance, &gpu_count, instance_physical_devices);
		assert(!error);

		vk_gpu = instance_physical_devices[0];
		delete[] instance_physical_devices;
	}
	// NOTE: We assume that the application runs on a single GPU computer. Otherwise, we would 
	//		 have to check our desired layers against the selected GPU available layers.

	// LOOK FOR DEVICE EXPECTED EXTENSION
	{
		uint32_t	device_extension_count = 0;

		error = vkEnumerateDeviceExtensionProperties(vk_gpu, nullptr, &device_extension_count, nullptr);
		assert(!error && device_extension_count);

		VkExtensionProperties* device_extensions = new VkExtensionProperties[device_extension_count];
		error = vkEnumerateDeviceExtensionProperties(vk_gpu, nullptr, &device_extension_count, device_extensions);
		assert(!error);

		bool extensions_ok = true;
		{
			for (uint32_t expected = 0; expected < ARRAY_SIZE(vk_device_extensions) && extensions_ok; ++expected)
			{
				char*	expected_extension = vk_device_extensions[expected];
				bool	expected_ok = false;
				for (uint32_t available = 0; available < device_extension_count && !expected_ok; ++available)
				{
					expected_ok = !strcmp(expected_extension, device_extensions[available].extensionName);
				}
				extensions_ok &= expected_ok;
			}
		}
		assert(extensions_ok);

		delete[] device_extensions;																	
	}

	// SELECT A QUEUE FROM THE PHYSICAL DEVICE
	{
		vkGetPhysicalDeviceQueueFamilyProperties(vk_gpu, &vk_queue_count, nullptr);
		assert(vk_queue_count > 0);

		vk_queue_props = new VkQueueFamilyProperties[vk_queue_count];
		vkGetPhysicalDeviceQueueFamilyProperties(vk_gpu, &vk_queue_count, vk_queue_props);

		uint32_t	elected_queue_index = 0;
		for (elected_queue_index = 0; elected_queue_index < vk_queue_count; elected_queue_index++)
		{
			if (vk_queue_props[elected_queue_index].queueFlags & VK_QUEUE_GRAPHICS_BIT)
				break;
		}
		assert(elected_queue_index < vk_queue_count);
	}
}


static void
vk_shutdown()
{
	delete[] vk_queue_props;
}

