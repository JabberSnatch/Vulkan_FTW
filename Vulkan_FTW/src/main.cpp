#include <windows.h>
#include <assert.h>

#include <vulkan/vulkan.h>

#include <iostream>
#include <vector>

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))

#define APP_NAME "Vulkan FTW"
#define ENGINE_NAME "YOLORenderSQUAD"

#define GET_INSTANCE_PROC_ADDR(inst, entrypoint)\
	{\
		fp.##entrypoint = (PFN_vk##entrypoint)vkGetInstanceProcAddr(inst, "vk" #entrypoint);\
	}

#define GET_DEVICE_PROC_ADDR(dev, entrypoint)\
	{\
		fp.##entrypoint = (PFN_vk##entrypoint)vkGetDeviceProcAddr(dev, "vk" #entrypoint);\
	}


static struct
{
	PFN_vkGetPhysicalDeviceSurfaceSupportKHR
		GetPhysicalDeviceSurfaceSupportKHR;
	PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR
		GetPhysicalDeviceSurfaceCapabilitiesKHR;
	PFN_vkGetPhysicalDeviceSurfaceFormatsKHR
		GetPhysicalDeviceSurfaceFormatsKHR;
	PFN_vkGetPhysicalDeviceSurfacePresentModesKHR
		GetPhysicalDeviceSurfacePresentModesKHR;

	PFN_vkCreateSwapchainKHR CreateSwapchainKHR;
	PFN_vkDestroySwapchainKHR DestroySwapchainKHR;
	PFN_vkGetSwapchainImagesKHR GetSwapchainImagesKHR;
	PFN_vkAcquireNextImageKHR AcquireNextImageKHR;
	PFN_vkQueuePresentKHR QueuePresentKHR;
} fp;



static HINSTANCE	win_instance;
static LPCSTR		win_class_name = "vulkan_render_window";
static LPCSTR		win_app_name = APP_NAME;
static int32_t		win_width = 800;
static int32_t		win_height = 600;
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
// NOTE: An Instance-enabled layer becomes available to the Device.
static std::vector<char*>		vk_enabled_layers;
// NOTE: Device and Instance have their separate list of extensions.
static std::vector<char*>		vk_enabled_extensions;

static VkInstance				vk_instance;

static VkPhysicalDevice					vk_gpu;
static VkPhysicalDeviceMemoryProperties	vk_memory_properties;

static uint32_t					vk_queue_family_count = 0;
static VkQueueFamilyProperties*	vk_queue_props = nullptr;
static uint32_t					vk_elected_queue_index;

static VkSurfaceKHR				vk_surface;
static VkFormat					vk_surface_format;
static VkColorSpaceKHR			vk_color_space;

static VkSwapchainKHR			vk_swapchain;

static VkDevice					vk_device;
static VkQueue					vk_main_queue;
static VkCommandPool			vk_cmd_pool;


static void create_window();
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
int main(int, char**);
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int);

static void vk_init();
static void vk_prepare_resources();
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
								   win_rect.right - win_rect.left, 
								   win_rect.bottom - win_rect.top,
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
	vk_prepare_resources();

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
					if (expected_ok) vk_enabled_layers.push_back(expected_layer);
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
					if (expected_ok) vk_enabled_extensions.push_back(expected_extension);
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
			instance_info.enabledLayerCount = (uint32_t)vk_enabled_layers.size();
			instance_info.ppEnabledLayerNames = vk_enabled_layers.data();
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

	// MAKE SURE THAT INSTANCE LAYER WERE GIVEN TO THE DEVICE
	{
		uint32_t	device_layer_count = 0;

		error = vkEnumerateDeviceLayerProperties(vk_gpu, &device_layer_count, 
												 nullptr);
		assert(!error);

		if (device_layer_count)
		{
			VkLayerProperties* device_layers = 
				new VkLayerProperties[device_layer_count];
			error = vkEnumerateDeviceLayerProperties(vk_gpu, 
													 &device_layer_count, 
													 device_layers);
			assert(!error);

			bool validation_ok = true;
			{
				for (uint32_t expected = 0; 
					 expected < (uint32_t)vk_enabled_layers.size() 
					 && validation_ok; 
					 ++expected)
				{
					char*	expected_layer = vk_enabled_layers[expected];
					bool	expected_ok = false;
					for (uint32_t available = 0; 
						 available < device_layer_count && !expected_ok; 
						 ++available)
					{
						expected_ok = !strcmp(expected_layer, 
											  device_layers[available].layerName);
					}
					validation_ok &= expected_ok;
				}
			}
			assert(validation_ok);
		}
	}


	// LOOK FOR DEVICE EXPECTED EXTENSION
	{
		vk_enabled_extensions.clear();
		uint32_t	device_extension_count = 0;

		error = vkEnumerateDeviceExtensionProperties(vk_gpu, nullptr, 
													 &device_extension_count, 
													 nullptr);
		assert(!error);

		if (device_extension_count)
		{
			VkExtensionProperties* device_extensions = 
				new VkExtensionProperties[device_extension_count];
			error = vkEnumerateDeviceExtensionProperties(vk_gpu, nullptr, 
														 &device_extension_count, 
														 device_extensions);
			assert(!error);

			bool extensions_ok = true;
			{
				for (uint32_t expected = 0; 
					 expected < ARRAY_SIZE(vk_device_extensions) && extensions_ok; 
					 ++expected)
				{
					char*	expected_extension = vk_device_extensions[expected];
					bool	expected_ok = false;
					for (uint32_t available = 0; 
						 available < device_extension_count && !expected_ok; 
						 ++available)
					{
						expected_ok = !strcmp(expected_extension, 
											  device_extensions[available].extensionName);
					}
					if (expected_ok) vk_enabled_extensions.push_back(expected_extension);
					extensions_ok &= expected_ok;
				}
			}
			assert(extensions_ok);

			delete[] device_extensions;																	
		}
	}

	// SELECT A QUEUE FROM THE PHYSICAL DEVICE
	{
		vkGetPhysicalDeviceQueueFamilyProperties(vk_gpu, &vk_queue_family_count, 
												 nullptr);
		assert(vk_queue_family_count > 0);

		vk_queue_props = new VkQueueFamilyProperties[vk_queue_family_count];
		vkGetPhysicalDeviceQueueFamilyProperties(vk_gpu, &vk_queue_family_count, 
												 vk_queue_props);
	}

	GET_INSTANCE_PROC_ADDR(vk_instance, GetPhysicalDeviceSurfaceSupportKHR);
	GET_INSTANCE_PROC_ADDR(vk_instance, GetPhysicalDeviceSurfaceCapabilitiesKHR);
	GET_INSTANCE_PROC_ADDR(vk_instance, GetPhysicalDeviceSurfaceFormatsKHR);
	GET_INSTANCE_PROC_ADDR(vk_instance, GetPhysicalDeviceSurfacePresentModesKHR);
	GET_INSTANCE_PROC_ADDR(vk_instance, GetSwapchainImagesKHR);

	// CREATE VKSURFACE
	{
		VkWin32SurfaceCreateInfoKHR		create_info;
		create_info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
		create_info.pNext = nullptr;
		create_info.flags = 0;
		create_info.hinstance = win_instance;
		create_info.hwnd = window_handle;

		error = vkCreateWin32SurfaceKHR(vk_instance, &create_info, nullptr, &vk_surface);
		assert(!error);
	}

	// PICK A QUEUE INDEX
	{
		VkBool32* supports_present = new VkBool32[vk_queue_family_count];
		for (uint32_t i = 0; i < vk_queue_family_count; ++i)
		{
			fp.GetPhysicalDeviceSurfaceSupportKHR(vk_gpu, i, vk_surface,
												  &supports_present[i]);
		}

		// Pick a queue that supports graphics and present
		uint32_t candidate_queue_index = UINT32_MAX;
		for (uint32_t i = 0; i < vk_queue_family_count; ++i)
		{
			if (vk_queue_props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT &&
				supports_present[i] == VK_TRUE)
			{
				candidate_queue_index = i;
				break;
			}
		}
		assert(candidate_queue_index != UINT32_MAX);

		vk_elected_queue_index = candidate_queue_index;

		delete[] supports_present;
	}

	// CREATE DEVICE
	{
		float queue_priorities[1] = { 0.0 };

		VkDeviceQueueCreateInfo queue_info;
		queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queue_info.pNext = nullptr;
		queue_info.queueFamilyIndex = vk_elected_queue_index;
		queue_info.queueCount = vk_queue_props[vk_elected_queue_index].queueCount;
		queue_info.pQueuePriorities = queue_priorities;

		VkDeviceCreateInfo device_info;
		device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		device_info.pNext = nullptr;
		device_info.queueCreateInfoCount = 1;
		device_info.pQueueCreateInfos = &queue_info;
		device_info.enabledLayerCount = (uint32_t)vk_enabled_layers.size();
		device_info.ppEnabledLayerNames = vk_enabled_layers.data();
		device_info.enabledExtensionCount = (uint32_t)vk_enabled_extensions.size();
		device_info.ppEnabledExtensionNames = vk_enabled_extensions.data();
		device_info.pEnabledFeatures = nullptr;

		error = vkCreateDevice(vk_gpu, &device_info, nullptr, &vk_device);
		assert(!error);
	}

	GET_DEVICE_PROC_ADDR(vk_device, CreateSwapchainKHR);
	GET_DEVICE_PROC_ADDR(vk_device, DestroySwapchainKHR);
	GET_DEVICE_PROC_ADDR(vk_device, GetSwapchainImagesKHR);
	GET_DEVICE_PROC_ADDR(vk_device, AcquireNextImageKHR);
	GET_DEVICE_PROC_ADDR(vk_device, QueuePresentKHR);

	// GET SURFACE FORMAT AND COLOR SPACE
	{
		uint32_t	format_count;
		error = fp.GetPhysicalDeviceSurfaceFormatsKHR(vk_gpu, vk_surface,
													  &format_count, nullptr);
		assert(!error);

		if (format_count)
		{
			VkSurfaceFormatKHR* surface_formats = new VkSurfaceFormatKHR[format_count];
			error = fp.GetPhysicalDeviceSurfaceFormatsKHR(vk_gpu, vk_surface,
														  &format_count, surface_formats);
			assert(!error && format_count);

			if (surface_formats[0].format == VK_FORMAT_UNDEFINED)
				vk_surface_format = VK_FORMAT_B8G8R8A8_UNORM;
			else
				vk_surface_format = surface_formats[0].format;

			vk_color_space = surface_formats[0].colorSpace;

			delete[] surface_formats;
		}
	}

	vkGetDeviceQueue(vk_device, vk_elected_queue_index, 0, &vk_main_queue);
	vkGetPhysicalDeviceMemoryProperties(vk_gpu, &vk_memory_properties);
}


static void
vk_prepare_resources()
{
	VkResult error;

	// CREATE SWAPCHAIN
	// NOTE: We assume that no swapchain is destroyed. If it turns out that we do see cube.c:890
	{

		VkSurfaceCapabilitiesKHR surface_capabilities;
		error = fp.GetPhysicalDeviceSurfaceCapabilitiesKHR(vk_gpu, vk_surface,
														   &surface_capabilities);
		assert(!error);

		VkExtent2D swapchain_extent;
		if (surface_capabilities.currentExtent.width == (uint32_t)-1)
		{
			swapchain_extent.width = win_width;
			swapchain_extent.height = win_height;
		}
		else
		{
			swapchain_extent = surface_capabilities.currentExtent;
			win_width = swapchain_extent.width;
			win_height = swapchain_extent.height;
		}

		uint32_t present_mode_count = 0;
		error = fp.GetPhysicalDeviceSurfacePresentModesKHR(vk_gpu, vk_surface,
														   &present_mode_count,
														   nullptr);
		assert(!error && present_mode_count);

		VkPresentModeKHR* present_modes = new VkPresentModeKHR[present_mode_count];
		error = fp.GetPhysicalDeviceSurfacePresentModesKHR(vk_gpu, vk_surface,
														   &present_mode_count,
														   present_modes);
		assert(!error);

		// NOTE: VK_PRESENT_MODE_FIFO_KHR is the default mode for any driver.
		//		 Also MAILBOX mode has v-sync. IMMEDIATE does not.
		VkPresentModeKHR expected_present_mode = VK_PRESENT_MODE_FIFO_KHR;
		for (uint32_t i = 0; i < present_mode_count; ++i)
		{
			if (present_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
			{
				expected_present_mode = VK_PRESENT_MODE_MAILBOX_KHR;
				break;
			}
			if (present_modes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR)
				expected_present_mode = VK_PRESENT_MODE_IMMEDIATE_KHR;
		}

		delete[] present_modes;

		// NOTE: We need an additional image because reasons.
		uint32_t expected_swapchain_images_count = surface_capabilities.minImageCount + 1;
		if (surface_capabilities.maxImageCount > 0 &&
			expected_swapchain_images_count > surface_capabilities.maxImageCount)
			expected_swapchain_images_count = surface_capabilities.maxImageCount;

		VkSurfaceTransformFlagBitsKHR pre_transform;
		if (surface_capabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
			pre_transform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
		else
			pre_transform = surface_capabilities.currentTransform;

		VkSwapchainCreateInfoKHR swapchain_info;
		swapchain_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		swapchain_info.pNext = nullptr;
		swapchain_info.flags = 0;
		swapchain_info.surface = vk_surface;
		swapchain_info.minImageCount = expected_swapchain_images_count;
		swapchain_info.imageFormat = vk_surface_format;
		swapchain_info.imageColorSpace = vk_color_space;
		swapchain_info.imageExtent = swapchain_extent;
		swapchain_info.imageArrayLayers = 1;
		swapchain_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		swapchain_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		swapchain_info.queueFamilyIndexCount = 0;
		swapchain_info.pQueueFamilyIndices = nullptr;
		swapchain_info.preTransform = pre_transform;
		swapchain_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		swapchain_info.presentMode = expected_present_mode;
		swapchain_info.clipped = VK_TRUE;
		swapchain_info.oldSwapchain = nullptr;

		error = fp.CreateSwapchainKHR(vk_device, &swapchain_info, nullptr, &vk_swapchain);
		assert(!error);
	}


	// CREATE COMMAND POOL
	{
		VkCommandPoolCreateInfo cmd_pool_info;
		cmd_pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		cmd_pool_info.pNext = nullptr;
		cmd_pool_info.flags = 0;
		cmd_pool_info.queueFamilyIndex = vk_elected_queue_index;
	
		error = vkCreateCommandPool(vk_device, &cmd_pool_info, nullptr, &vk_cmd_pool);
		assert(!error);
	}

	


}


static void
vk_shutdown()
{
	delete[] vk_queue_props;
}

