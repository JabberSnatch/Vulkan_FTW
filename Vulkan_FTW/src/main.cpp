#include <windows.h>
#include <assert.h>

#include <vulkan/vulkan.h>

#include <iostream>
#include <vector>
#include <unordered_map>
#include <string>
#include <fstream>

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))

#define APP_NAME "Vulkan FTW"
#define ENGINE_NAME "YOLORenderSQUAD"

#define GET_INSTANCE_PROC_ADDR(inst, entrypoint)\
	{\
		fp.##entrypoint = \
			(PFN_vk##entrypoint)vkGetInstanceProcAddr(inst, "vk" #entrypoint);\
		assert(fp.##entrypoint != nullptr);\
	}

#define GET_DEVICE_PROC_ADDR(dev, entrypoint)\
	{\
		fp.##entrypoint = \
			(PFN_vk##entrypoint)vkGetDeviceProcAddr(dev, "vk" #entrypoint);\
		assert(fp.##entrypoint != nullptr);\
	}

using Bitfield32_t = uint32_t;


struct SwapchainBuffer
{
	VkImage				image;
	VkCommandBuffer		cmd;
	VkImageView			view;
};

struct DepthBuffer
{
	VkFormat				format;
	VkImage					image;
	VkImageView				view;
	VkMemoryAllocateInfo	mem_alloc_info;
	VkDeviceMemory			memory;
};



static struct
{
	PFN_vkGetPhysicalDeviceSurfaceSupportKHR
		GetPhysicalDeviceSurfaceSupportKHR = nullptr;
	PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR
		GetPhysicalDeviceSurfaceCapabilitiesKHR = nullptr;
	PFN_vkGetPhysicalDeviceSurfaceFormatsKHR
		GetPhysicalDeviceSurfaceFormatsKHR = nullptr;
	PFN_vkGetPhysicalDeviceSurfacePresentModesKHR
		GetPhysicalDeviceSurfacePresentModesKHR = nullptr;

	PFN_vkCreateSwapchainKHR CreateSwapchainKHR = nullptr;
	PFN_vkDestroySwapchainKHR DestroySwapchainKHR = nullptr;
	PFN_vkGetSwapchainImagesKHR GetSwapchainImagesKHR = nullptr;
	PFN_vkAcquireNextImageKHR AcquireNextImageKHR = nullptr;
	PFN_vkQueuePresentKHR QueuePresentKHR = nullptr;

	PFN_vkCreateDebugReportCallbackEXT CreateDebugReportCallbackEXT = nullptr;
	PFN_vkDestroyDebugReportCallbackEXT DestroyDebugReportCallbackEXT = nullptr;
	PFN_vkDebugReportMessageEXT DebugReportMessageEXT = nullptr;
} fp;



static HINSTANCE	win_instance;
static LPCSTR		win_class_name = "vulkan_render_window";
static LPCSTR		win_app_name = APP_NAME;
static uint32_t		win_width = 800;
static uint32_t		win_height = 600;
static HWND			window_handle;


// Tells the application if it should load Vulkan's validation layers.
static bool			vk_validate = true; 
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
static uint32_t					vk_swapchain_image_count;
static SwapchainBuffer*			vk_swapchain_buffers;

static DepthBuffer				vk_depth_buffer;

static VkDevice					vk_device;
static VkQueue					vk_main_queue;
static VkCommandPool			vk_cmd_pool;
static VkCommandBuffer			vk_cmd_buffer;

static VkPipelineLayout			vk_pipeline_layout;
static VkRenderPass				vk_render_pass;
static VkPipelineCache			vk_pipeline_cache;
static VkPipeline				vk_pipeline;

static VkFramebuffer*			vk_framebuffers;

static VkDescriptorSetLayout	vk_desc_set_layout;
static VkDescriptorPool			vk_descriptor_pool;
static VkDescriptorSet			vk_descriptor_set;

static std::unordered_map<std::string, VkShaderModule>		vk_shaders;

static VkBuffer					vk_matrix_buffer;
static VkMemoryAllocateInfo		vk_matrix_buffer_mem_alloc;
static VkDeviceMemory			vk_matrix_buffer_memory;
static VkDescriptorBufferInfo	vk_matrix_buffer_desc_info;


static VkDebugReportCallbackEXT	vk_debug_callback;


static void create_window();
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
int main(int, char**);
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int);

static void vk_init();
static void vk_setup_debug_report_callback();
static void vk_prepare_resources();
static void vk_prepare_pipeline();
static void vk_shutdown();

static void vk_set_image_layout(VkImage, VkImageAspectFlags, VkImageLayout, 
								VkImageLayout, VkAccessFlagBits);

static VkShaderModule vk_load_shader(const std::string&, const std::string&);

static uint32_t	vk_get_memory_type_index(const VkPhysicalDeviceMemoryProperties&, 
										 Bitfield32_t, VkFlags);

VKAPI_ATTR VkBool32 VKAPI_CALL
vk_debug_log(VkFlags, VkDebugReportObjectTypeEXT, uint64_t, size_t, int32_t,
			 const char*, const char*, void*);

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

	RECT win_rect = { 0, 0, (int32_t)win_width, (int32_t)win_height };
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
	vk_prepare_pipeline();

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

	if (vk_validate)
		vk_setup_debug_report_callback();

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
		queue_info.flags = 0;
		queue_info.queueFamilyIndex = vk_elected_queue_index;
		queue_info.queueCount = 1;
		queue_info.pQueuePriorities = queue_priorities;

		VkDeviceCreateInfo device_info;
		device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		device_info.pNext = nullptr;
		device_info.flags = 0;
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
vk_setup_debug_report_callback()
{
	VkResult error;

	GET_INSTANCE_PROC_ADDR(vk_instance, CreateDebugReportCallbackEXT);
	GET_INSTANCE_PROC_ADDR(vk_instance, DestroyDebugReportCallbackEXT);
	GET_INSTANCE_PROC_ADDR(vk_instance, DebugReportMessageEXT);

	VkDebugReportCallbackCreateInfoEXT debug_callback_info;
	debug_callback_info.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT;
	debug_callback_info.pNext = nullptr;
	debug_callback_info.pfnCallback = vk_debug_log;
	debug_callback_info.pUserData = nullptr;
	debug_callback_info.flags = 
		VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT;
	error = fp.CreateDebugReportCallbackEXT(vk_instance, &debug_callback_info,
											nullptr, &vk_debug_callback);
	assert(!error);
}


static void
vk_prepare_resources()
{
	VkResult error;
	
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

	// CREATE SWAPCHAIN IMAGES
	{
		error = fp.GetSwapchainImagesKHR(vk_device, vk_swapchain, 
										 &vk_swapchain_image_count, nullptr);  
		assert(!error && vk_swapchain_image_count);

		VkImage* swapchain_images = new VkImage[vk_swapchain_image_count];
		error = fp.GetSwapchainImagesKHR(vk_device, vk_swapchain,
										 &vk_swapchain_image_count, swapchain_images);
		assert(!error);

		vk_swapchain_buffers = new SwapchainBuffer[vk_swapchain_image_count];

		for (uint32_t i = 0; i < vk_swapchain_image_count; ++i)
		{
			vk_swapchain_buffers[i].image = swapchain_images[i];

			VkImageViewCreateInfo image_view_info;
			image_view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			image_view_info.pNext = nullptr;
			image_view_info.flags = 0;
			image_view_info.image = vk_swapchain_buffers[i].image;
			image_view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
			image_view_info.format = vk_surface_format;
			image_view_info.components = {
				VK_COMPONENT_SWIZZLE_R,
				VK_COMPONENT_SWIZZLE_G,
				VK_COMPONENT_SWIZZLE_B,
				VK_COMPONENT_SWIZZLE_A
			};
			image_view_info.subresourceRange = {
				VK_IMAGE_ASPECT_COLOR_BIT,
				0, 1, 0, 1
			};

			error = vkCreateImageView(vk_device, &image_view_info, nullptr, 
									  &vk_swapchain_buffers[i].view);
			assert(!error);
		}

		delete[] swapchain_images;
	}

	// CREATE DEPTH BUFFER
	{
		VkFormat			depth_format = VK_FORMAT_D16_UNORM;
		vk_depth_buffer.format = depth_format;

		VkImageCreateInfo	image_info;
		image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		image_info.pNext = nullptr;
		image_info.flags = 0;
		image_info.imageType = VK_IMAGE_TYPE_2D;
		image_info.format = depth_format;
		image_info.extent = { win_width, win_height, 1 };
		image_info.mipLevels = 1;
		image_info.arrayLayers = 1;
		image_info.samples = VK_SAMPLE_COUNT_1_BIT;
		image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
		image_info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		image_info.queueFamilyIndexCount = 0;
		image_info.pQueueFamilyIndices = nullptr;
		// NOTE: There might be some better options out there
		image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; 

		error = vkCreateImage(vk_device, &image_info, nullptr, &vk_depth_buffer.image);
		assert(!error);

		VkMemoryRequirements image_mem_reqs;
		vkGetImageMemoryRequirements(vk_device, vk_depth_buffer.image, &image_mem_reqs);

		// DEPTH BUFFER MEMORY ALLOCATION
		vk_depth_buffer.mem_alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		vk_depth_buffer.mem_alloc_info.pNext = nullptr;
		vk_depth_buffer.mem_alloc_info.allocationSize = image_mem_reqs.size;
		vk_depth_buffer.mem_alloc_info.memoryTypeIndex =
			vk_get_memory_type_index(vk_memory_properties,
									 image_mem_reqs.memoryTypeBits,
									 0);
		assert(vk_depth_buffer.mem_alloc_info.memoryTypeIndex != UINT32_MAX);

		error = vkAllocateMemory(vk_device, &vk_depth_buffer.mem_alloc_info,
								 nullptr, &vk_depth_buffer.memory);
		assert(!error);

		error = vkBindImageMemory(vk_device, vk_depth_buffer.image, 
								  vk_depth_buffer.memory, 0);
		assert(!error);

		vk_set_image_layout(vk_depth_buffer.image, VK_IMAGE_ASPECT_DEPTH_BIT,
							VK_IMAGE_LAYOUT_UNDEFINED,
							VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
							(VkAccessFlagBits)0);

		VkImageViewCreateInfo	view_info;
		view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		view_info.pNext = nullptr;
		view_info.flags = 0;
		view_info.image = vk_depth_buffer.image;
		view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
		view_info.format = depth_format;
		view_info.components = {
			VK_COMPONENT_SWIZZLE_IDENTITY,
			VK_COMPONENT_SWIZZLE_IDENTITY,
			VK_COMPONENT_SWIZZLE_IDENTITY,
			VK_COMPONENT_SWIZZLE_IDENTITY
		};
		view_info.subresourceRange = { 
			VK_IMAGE_ASPECT_DEPTH_BIT,
			0, 1, 0, 1
		};

		error = vkCreateImageView(vk_device, &view_info, nullptr, 
								  &vk_depth_buffer.view);
		assert(!error);
	}

	// CREATE SWAPCHAIN CMD BUFFERS
	{
		VkCommandBufferAllocateInfo cmd_info;
		cmd_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		cmd_info.pNext = nullptr;
		cmd_info.commandPool = vk_cmd_pool;
		cmd_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		cmd_info.commandBufferCount = 1;
	
		for (uint32_t i = 0; i < vk_swapchain_image_count; ++i)
		{
			error = vkAllocateCommandBuffers(vk_device, &cmd_info,
											&vk_swapchain_buffers[i].cmd);
			assert(!error);
		}
	}

	// CREATE UNIFORM BUFFER
	{
		VkBufferCreateInfo buffer_info;
		buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		buffer_info.pNext = nullptr;
		buffer_info.flags = 0;
		buffer_info.size = 16 * 3 * sizeof(float);
		buffer_info.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
		buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		buffer_info.queueFamilyIndexCount = 0;
		buffer_info.pQueueFamilyIndices = nullptr;

		error = vkCreateBuffer(vk_device, &buffer_info, nullptr,
							   &vk_matrix_buffer);
		assert(!error);

		VkMemoryRequirements memory_reqs;
		vkGetBufferMemoryRequirements(vk_device, vk_matrix_buffer, 
									  &memory_reqs);

		// NOTE: vk_matrix_buffer_mem_alloc might be unnecessary, as VK_WHOLE_SIZE
		//		 is a valid argument to vkMapMemory size parameter.
		vk_matrix_buffer_mem_alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		vk_matrix_buffer_mem_alloc.pNext = nullptr;
		vk_matrix_buffer_mem_alloc.allocationSize = memory_reqs.size;
		vk_matrix_buffer_mem_alloc.memoryTypeIndex = 
			vk_get_memory_type_index(vk_memory_properties, 
									 memory_reqs.memoryTypeBits,
									 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | 
									 VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		assert(vk_matrix_buffer_mem_alloc.memoryTypeIndex != UINT32_MAX);

		error = vkAllocateMemory(vk_device, &vk_matrix_buffer_mem_alloc, nullptr,
								 &vk_matrix_buffer_memory);
		assert(!error);

		uint8_t* p_data;
		error = vkMapMemory(vk_device, vk_matrix_buffer_memory, 0,
							VK_WHOLE_SIZE, 0,
							(void**)&p_data);
		assert(!error);

		memset(p_data, 0, buffer_info.size);

		vkUnmapMemory(vk_device, vk_matrix_buffer_memory);

		error = vkBindBufferMemory(vk_device, vk_matrix_buffer,
								   vk_matrix_buffer_memory, 0);
		assert(!error);

		// NOTE: vk_matrix_buffer_desc_info might be created in place, 
		//		 if its range is always VK_WHOLE_SIZE
		vk_matrix_buffer_desc_info.buffer = vk_matrix_buffer;
		vk_matrix_buffer_desc_info.offset = 0;
		vk_matrix_buffer_desc_info.range = VK_WHOLE_SIZE;
	}
}


static void
vk_prepare_pipeline()
{
	VkResult error;

	// DESCRIPTOR SET LAYOUT
	{
		VkDescriptorSetLayoutBinding layout_bindings[2];
		layout_bindings[0].binding = 0;
		layout_bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		layout_bindings[0].descriptorCount = 1;
		layout_bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
		layout_bindings[0].pImmutableSamplers = nullptr;

		layout_bindings[1].binding = 1;
		layout_bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		layout_bindings[1].descriptorCount = 1;
		layout_bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		layout_bindings[1].pImmutableSamplers = nullptr;
	
		VkDescriptorSetLayoutCreateInfo desc_layout_info;
		desc_layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		desc_layout_info.pNext = nullptr;
		desc_layout_info.flags = 0;
		desc_layout_info.bindingCount = 1;
		desc_layout_info.pBindings = layout_bindings;

		error = vkCreateDescriptorSetLayout(vk_device, &desc_layout_info,
											nullptr, &vk_desc_set_layout);
		assert(!error);

		// NOTE: vk_pipeline_layout is not used before the pipeline creation,
		//		 so it could be created later.
		VkPipelineLayoutCreateInfo pipeline_layout_info;
		pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipeline_layout_info.pNext = nullptr;
		pipeline_layout_info.flags = 0;
		pipeline_layout_info.setLayoutCount = 1;
		pipeline_layout_info.pSetLayouts = &vk_desc_set_layout;
		pipeline_layout_info.pushConstantRangeCount = 0;
		pipeline_layout_info.pPushConstantRanges = nullptr;

		error = vkCreatePipelineLayout(vk_device, &pipeline_layout_info, nullptr,
									   &vk_pipeline_layout);
		assert(!error);
	}

	// CREATE DESCRIPTOR POOL
	{
		VkDescriptorPoolSize desc_counts[2];
		desc_counts[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		desc_counts[0].descriptorCount = 1;
		desc_counts[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		desc_counts[1].descriptorCount = 1;

		VkDescriptorPoolCreateInfo desc_pool_info;
		desc_pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		desc_pool_info.pNext = nullptr;
		desc_pool_info.flags = 0;
		desc_pool_info.maxSets = 1;
		desc_pool_info.poolSizeCount = 1;
		desc_pool_info.pPoolSizes = desc_counts;

		error = vkCreateDescriptorPool(vk_device, &desc_pool_info, nullptr,
									   &vk_descriptor_pool);
		assert(!error);
	}

	// ALLOCATE DESCRIPTOR SET
	{
		VkDescriptorSetAllocateInfo alloc_info;
		alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		alloc_info.pNext = nullptr;
		alloc_info.descriptorPool = vk_descriptor_pool;
		alloc_info.descriptorSetCount = 1;
		alloc_info.pSetLayouts = &vk_desc_set_layout;
		
		error = vkAllocateDescriptorSets(vk_device, &alloc_info, 
										 &vk_descriptor_set);
		assert(!error);

		// NOTE: See cube:demo_prepare_descriptor_set:1737 for texture handling.
		VkDescriptorBufferInfo buffer_desc_info;
		buffer_desc_info.buffer = vk_matrix_buffer;
		buffer_desc_info.offset = 0;
		buffer_desc_info.range = VK_WHOLE_SIZE;
		VkWriteDescriptorSet desc_set_writes[1];
		desc_set_writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		desc_set_writes[0].pNext = nullptr;
		desc_set_writes[0].dstSet = vk_descriptor_set;
		desc_set_writes[0].dstBinding = 0;
		desc_set_writes[0].dstArrayElement = 0;
		desc_set_writes[0].descriptorCount = 1;
		// NOTE: Remember to look into VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC
		desc_set_writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		desc_set_writes[0].pImageInfo = nullptr;
		desc_set_writes[0].pBufferInfo = &buffer_desc_info;
		desc_set_writes[0].pTexelBufferView = nullptr;

		vkUpdateDescriptorSets(vk_device, 1, desc_set_writes, 0, nullptr);
	}


	// RENDER PASS
	{
		VkAttachmentDescription attachments[2];
		attachments[0].flags = 0;
		attachments[0].format = vk_surface_format;
		attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
		attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[0].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		attachments[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		attachments[1].flags = 0;
		attachments[1].format = vk_depth_buffer.format;
		attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
		attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[1].initialLayout = 
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		attachments[1].finalLayout = 
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkAttachmentReference color_reference;
		color_reference.attachment = 0;
		color_reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkAttachmentReference depth_reference;
		depth_reference.attachment = 1;
		depth_reference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkSubpassDescription subpass;
		subpass.flags = 0;
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.inputAttachmentCount = 0;
		subpass.pInputAttachments = nullptr;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &color_reference;
		subpass.pResolveAttachments = nullptr;
		subpass.pDepthStencilAttachment = &depth_reference;
		subpass.preserveAttachmentCount = 0;
		subpass.pPreserveAttachments = nullptr;

		VkRenderPassCreateInfo renderpass_info;
		renderpass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderpass_info.pNext = nullptr;
		renderpass_info.flags = 0;
		renderpass_info.attachmentCount = 2;
		renderpass_info.pAttachments = attachments;
		renderpass_info.subpassCount = 1;
		renderpass_info.pSubpasses = &subpass;
		renderpass_info.dependencyCount = 0;
		renderpass_info.pDependencies = nullptr;

		error = vkCreateRenderPass(vk_device, &renderpass_info, nullptr,
								   &vk_render_pass);
		assert(!error);
	}

	// CREATE FRAMEBUFFERS
	{
		VkImageView attachments[2];
		attachments[1] = vk_depth_buffer.view;

		VkFramebufferCreateInfo framebuffer_info;
		framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebuffer_info.pNext = nullptr;
		framebuffer_info.flags = 0;
		framebuffer_info.renderPass = vk_render_pass;
		framebuffer_info.attachmentCount = 2;
		framebuffer_info.pAttachments = attachments;
		framebuffer_info.width = win_width;
		framebuffer_info.height = win_height;
		framebuffer_info.layers = 1;

		vk_framebuffers = new VkFramebuffer[vk_swapchain_image_count];

		for (uint32_t i = 0; i < vk_swapchain_image_count; ++i)
		{
			attachments[0] = vk_swapchain_buffers[i].view;
			error = vkCreateFramebuffer(vk_device, &framebuffer_info, nullptr,
										&vk_framebuffers[i]);
			assert(!error);
		}
	}
	
	// PIPELINE
	{
		VkPipelineVertexInputStateCreateInfo	vi_info;
		VkPipelineInputAssemblyStateCreateInfo	ia_info;
		VkPipelineViewportStateCreateInfo		vp_info;
		VkPipelineRasterizationStateCreateInfo	rs_info;
		VkPipelineMultisampleStateCreateInfo	ms_info;
		VkPipelineDepthStencilStateCreateInfo	ds_info;
		VkPipelineColorBlendStateCreateInfo		cb_info;
		VkPipelineDynamicStateCreateInfo		dy_info;

		vi_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vi_info.pNext = nullptr;
		vi_info.flags = 0;
		vi_info.vertexBindingDescriptionCount = 0;
		vi_info.pVertexBindingDescriptions = nullptr;
		vi_info.vertexAttributeDescriptionCount = 0;
		vi_info.pVertexAttributeDescriptions = nullptr;

		ia_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		ia_info.pNext = nullptr;
		ia_info.flags = 0;
		ia_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		ia_info.primitiveRestartEnable = VK_FALSE;

		vp_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		vp_info.pNext = nullptr;
		vp_info.flags = 0;
		vp_info.viewportCount = 1;
		vp_info.pViewports = nullptr;
		vp_info.scissorCount = 1;
		vp_info.pScissors = nullptr;

		// NOTE: There used to be a memset(0) here.
		rs_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rs_info.pNext = nullptr;
		rs_info.flags = 0;
		rs_info.depthClampEnable = VK_FALSE;
		rs_info.rasterizerDiscardEnable = VK_FALSE;
		rs_info.polygonMode = VK_POLYGON_MODE_FILL;
		rs_info.cullMode = VK_CULL_MODE_BACK_BIT;
		rs_info.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
		rs_info.depthBiasEnable = VK_FALSE;
		rs_info.depthBiasConstantFactor = 0.f;
		rs_info.depthBiasClamp = 0.f;
		rs_info.depthBiasSlopeFactor = 0.f;
		rs_info.lineWidth = 1.0f;

		ms_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		ms_info.pNext = nullptr;
		ms_info.flags = 0;
		ms_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		ms_info.sampleShadingEnable = VK_FALSE;
		ms_info.minSampleShading = 0.f;
		ms_info.pSampleMask = nullptr;
		ms_info.alphaToCoverageEnable = VK_FALSE;
		ms_info.alphaToOneEnable = VK_FALSE;

		ds_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		ds_info.pNext = nullptr;
		ds_info.flags = 0;
		ds_info.depthTestEnable = VK_TRUE;
		ds_info.depthWriteEnable = VK_TRUE;
		ds_info.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
		ds_info.depthBoundsTestEnable = VK_FALSE;
		ds_info.stencilTestEnable = VK_FALSE;
		ds_info.front.failOp = VK_STENCIL_OP_KEEP;
		ds_info.front.passOp = VK_STENCIL_OP_KEEP;
		ds_info.front.compareOp = VK_COMPARE_OP_NEVER;
		ds_info.front.depthFailOp = VK_STENCIL_OP_KEEP;
		ds_info.front.compareMask = 0;
		ds_info.front.writeMask = 0;
		ds_info.front.reference = 0;
		ds_info.back = ds_info.front;
		ds_info.minDepthBounds = 0.f;
		ds_info.maxDepthBounds = 0.f;

		VkPipelineColorBlendAttachmentState attachment_state;
		attachment_state.blendEnable = VK_FALSE;
		attachment_state.srcColorBlendFactor = VK_BLEND_FACTOR_ZERO;
		attachment_state.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
		attachment_state.colorBlendOp = VK_BLEND_OP_ADD;
		attachment_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		attachment_state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		attachment_state.alphaBlendOp = VK_BLEND_OP_ADD;
		attachment_state.colorWriteMask =
			VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
			VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		cb_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		cb_info.pNext = nullptr;
		cb_info.flags = 0;
		cb_info.logicOpEnable = VK_FALSE;
		cb_info.logicOp = VK_LOGIC_OP_CLEAR;
		cb_info.attachmentCount = 1;
		cb_info.pAttachments = &attachment_state;
		cb_info.blendConstants[0] = 0;
		cb_info.blendConstants[1] = 0;
		cb_info.blendConstants[2] = 0;
		cb_info.blendConstants[3] = 0;

		VkDynamicState dynamic_states[VK_DYNAMIC_STATE_RANGE_SIZE];
		dynamic_states[0] = VK_DYNAMIC_STATE_VIEWPORT;
		dynamic_states[1] = VK_DYNAMIC_STATE_SCISSOR;
		dy_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dy_info.pNext = nullptr;
		dy_info.flags = 0;
		dy_info.dynamicStateCount = 2;
		dy_info.pDynamicStates = dynamic_states;

		VkPipelineShaderStageCreateInfo pipeline_stages[2];
		pipeline_stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		pipeline_stages[0].pNext = nullptr;
		pipeline_stages[0].flags = 0;
		pipeline_stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
		pipeline_stages[0].module = vk_load_shader("vs_test", 
												   "Resources/vs_test.spv");
		pipeline_stages[0].pName = "main";
		pipeline_stages[0].pSpecializationInfo = nullptr;
		pipeline_stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		pipeline_stages[1].pNext = nullptr;
		pipeline_stages[1].flags = 0;
		pipeline_stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		pipeline_stages[1].module = vk_load_shader("fs_test", 
												   "Resources/fs_test.spv");
		pipeline_stages[1].pName = "main";
		pipeline_stages[1].pSpecializationInfo = nullptr;
		VkGraphicsPipelineCreateInfo pipeline_info;
		pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipeline_info.pNext = nullptr;
		pipeline_info.flags = 0;
		pipeline_info.stageCount = 2;
		pipeline_info.pStages = pipeline_stages;
		pipeline_info.pVertexInputState = &vi_info;
		pipeline_info.pInputAssemblyState = &ia_info;
		pipeline_info.pTessellationState = nullptr;
		pipeline_info.pViewportState = &vp_info;
		pipeline_info.pRasterizationState = &rs_info;
		pipeline_info.pMultisampleState = &ms_info;
		pipeline_info.pDepthStencilState = &ds_info;
		pipeline_info.pColorBlendState = &cb_info;
		pipeline_info.pDynamicState = &dy_info;
		pipeline_info.layout = vk_pipeline_layout;
		pipeline_info.renderPass = vk_render_pass;
		pipeline_info.subpass = 0;
		pipeline_info.basePipelineHandle = VK_NULL_HANDLE;
		pipeline_info.basePipelineIndex = -1;

		VkPipelineCacheCreateInfo pipeline_cache_info;
		pipeline_cache_info.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
		pipeline_cache_info.pNext = nullptr;
		pipeline_cache_info.flags = 0;
		pipeline_cache_info.initialDataSize = 0;
		pipeline_cache_info.pInitialData = nullptr;
		error = vkCreatePipelineCache(vk_device, &pipeline_cache_info, nullptr,
									  &vk_pipeline_cache);
		assert(!error);

		error = vkCreateGraphicsPipelines(vk_device, vk_pipeline_cache, 1, 
										  &pipeline_info, nullptr,
										  &vk_pipeline);
		assert(!error);

		// NOTE: Once the graphics pipeline has been created, the shader modules
		//		 should be destroyable.
	}
}


static void
vk_shutdown()
{
	for (int i = 0; i < vk_swapchain_image_count; ++i)
	{
		vkDestroyFramebuffer(vk_device, vk_framebuffers[i], nullptr);
	}
	delete[] vk_framebuffers;

	delete[] vk_swapchain_buffers;
	delete[] vk_queue_props;

	vkDestroyCommandPool(vk_device, vk_cmd_pool, nullptr);
	vkDestroyDevice(vk_device, nullptr);

	if (vk_validate)
		fp.DestroyDebugReportCallbackEXT(vk_instance, 
										 vk_debug_callback, nullptr);

	vkDestroySurfaceKHR(vk_instance, vk_surface, nullptr);
	vkDestroyInstance(vk_instance, nullptr);
}


static void 
vk_set_image_layout(VkImage image, VkImageAspectFlags aspect_mask, 
					VkImageLayout old_layout, VkImageLayout new_layout,
					VkAccessFlagBits access_mask)
{
	VkResult error;

	if (vk_cmd_buffer == VK_NULL_HANDLE)
	{
		VkCommandBufferAllocateInfo cmd_allocate_info;
		cmd_allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		cmd_allocate_info.pNext = nullptr;
		cmd_allocate_info.commandPool = vk_cmd_pool;
		cmd_allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		cmd_allocate_info.commandBufferCount = 1;

		error = vkAllocateCommandBuffers(vk_device, 
										 &cmd_allocate_info, 
										 &vk_cmd_buffer);
		assert(!error);

		VkCommandBufferBeginInfo cmd_begin_info;
		cmd_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		cmd_begin_info.pNext = nullptr;
		cmd_begin_info.flags = 0;
		cmd_begin_info.pInheritanceInfo = nullptr;

		error = vkBeginCommandBuffer(vk_cmd_buffer, &cmd_begin_info);
		assert(!error);
	}

	VkImageMemoryBarrier image_memory_barrier;
	image_memory_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	image_memory_barrier.pNext = nullptr;
	image_memory_barrier.srcAccessMask = access_mask;
	image_memory_barrier.dstAccessMask = 0;
	image_memory_barrier.oldLayout = old_layout;
	image_memory_barrier.newLayout = new_layout;
	image_memory_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	image_memory_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	image_memory_barrier.image = image;
	image_memory_barrier.subresourceRange = { aspect_mask, 0, 1, 0, 1 };

	switch (new_layout)
	{
	case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
		image_memory_barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		break;
	case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
		image_memory_barrier.dstAccessMask = 
			VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		break;
	case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
		image_memory_barrier.dstAccessMask =
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		break;
	case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
		image_memory_barrier.dstAccessMask = 
			VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
		break;
	default: break;
	}

	vkCmdPipelineBarrier(vk_cmd_buffer, 
						 VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
						 VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
						 0, 
						 0, nullptr, 
						 0, nullptr, 
						 1, &image_memory_barrier);
}


static VkShaderModule
vk_load_shader(const std::string& _name, const std::string& _path)
{
	long int size;
	size_t retval;
	char* buffer;

	FILE *fp = fopen(_path.c_str(), "rb");
	if (!fp)
		return NULL;

	fseek(fp, 0L, SEEK_END);
	size = ftell(fp);

	fseek(fp, 0L, SEEK_SET);

	buffer = new char[size];
	retval = fread(buffer, size, 1, fp);
	assert(retval == 1);

	fclose(fp);


	VkResult error;
	
	VkShaderModule shader_module;
	VkShaderModuleCreateInfo shader_info;
	shader_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	shader_info.pNext = nullptr;
	shader_info.flags = 0;
	shader_info.codeSize = static_cast<size_t>(size);
	shader_info.pCode = reinterpret_cast<uint32_t*>(buffer);

	error = vkCreateShaderModule(vk_device, &shader_info, nullptr, 
								 &shader_module);
	assert(!error);

	vk_shaders.emplace(_name, shader_module);
	return shader_module;
}


static uint32_t
vk_get_memory_type_index(const VkPhysicalDeviceMemoryProperties& memory_properties,
						 Bitfield32_t type_bits, VkFlags type_requirements)
{
	uint32_t result = UINT32_MAX;
	for (uint32_t i = 0; i < memory_properties.memoryTypeCount; ++i)
	{
		VkFlags matching_flags =
			memory_properties.memoryTypes[i].propertyFlags & type_requirements;
		
		if ((type_bits & (1 << i)) && matching_flags == type_requirements)
		{
			result = i;
			break;
		}
	}
	return result;
}


VKAPI_ATTR VkBool32 VKAPI_CALL
vk_debug_log(VkFlags msg_flags,
			 VkDebugReportObjectTypeEXT obj_type,
			 uint64_t src_object,
			 size_t location, 
			 int32_t msg_code,
			 const char* p_layer_prefix, 
			 const char* p_msg, 
			 void* p_user_data)
{
	std::string message = "[";
	message.append(p_layer_prefix);
	message += "] Code ";
	message.append(std::to_string(msg_code));
	message += " : ";
	message.append(p_msg);

	std::string prefix = "";
	if (msg_flags & VK_DEBUG_REPORT_ERROR_BIT_EXT)
		prefix = "[ERROR] ";
	else if (msg_flags & VK_DEBUG_REPORT_WARNING_BIT_EXT)
		prefix = "[WARNING] ";

	std::cout << prefix << message << std::endl;

	/*
	* false indicates that layer should not bail-out of an
	* API call that had validation failures. This may mean that the
	* app dies inside the driver due to invalid parameter(s).
	* That's what would happen without validation layers, so we'll
	* keep that behavior here.
	*/
	return false;
}


