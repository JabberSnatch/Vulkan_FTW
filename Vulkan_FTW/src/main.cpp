#include <windows.h>
#include <assert.h>

#include <vulkan/vulkan.h>

#include <iostream>

static HINSTANCE	win_instance;
static LPCSTR		win_class_name = "vulkan_render_window";
static LPCSTR		win_app_name = "Vulkan FTW";
static LONG			win_width = 800;
static LONG			win_height = 640;
static HWND			window_handle;

static bool			vk_validate = true; // Tells the application if it should load Vulkan's validation layers.


static void create_window();
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
int main(int, char**);
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int);

static void vk_init();


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

	return (int)msg.wParam;
}


static void
vk_init()
{
	VkResult	error;
	uint32_t	instance_layer_count = 0;

	if (vk_validate)
	{
		error = vkEnumerateInstanceLayerProperties(&instance_layer_count, nullptr);
		assert(!error);

		if (instance_layer_count > 0)
		{
			VkLayerProperties* instance_layers = new VkLayerProperties[instance_layer_count];

			error = vkEnumerateInstanceLayerProperties(&instance_layer_count, instance_layers);
			assert(!error);

			for (uint32_t i = 0; i < instance_layer_count; ++i)
			{
				VkLayerProperties& layer_prop = instance_layers[i];
				std::cout << layer_prop.layerName << " : " << layer_prop.description << std::endl;
			}

			delete[] instance_layers;
		}
	}
}

