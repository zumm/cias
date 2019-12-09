#include <iostream>
#include <vector>
#include <fstream>
#include <string>
#include <chrono>

#include <Windows.h>
#include <gdiplus.h>


class kernel_failure:
    public std::runtime_error
{
public:
    kernel_failure(const std::string & method_name):
        std::runtime_error("Method [" + method_name + "] failed with code [" + std::to_string(GetLastError()) + "].\n")
    {}
};

class hresult_failure:
        public std::runtime_error
{
public:
    hresult_failure(const std::string & method_name, HRESULT error_code):
        std::runtime_error("Method [" + method_name + "] failed with code [" + std::to_string(error_code) + "].\n")
    {}
};

class gdi_failure:
        public std::runtime_error
{
public:
    gdi_failure(const std::string & method_name, Gdiplus::Status error_code):
        std::runtime_error("Method [" + method_name + "] failed with code [" + std::to_string(error_code) + "].\n")
    {}
};


static const char * class_name = "CIAS_CLASS";

std::string save_path;
std::vector<uint8_t> old_image_data;
std::vector<uint8_t> new_image_data;


uint64_t get_timestamp()
{
    return (uint64_t) std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

LRESULT CALLBACK window_procedure(HWND handle, UINT message, WPARAM wp, LPARAM lp)
{
    if(WM_CLIPBOARDUPDATE != message || !IsClipboardFormatAvailable(CF_BITMAP))
    {
        return DefWindowProc(handle, message, wp, lp);
    }

    std::cout << "Clipboard receives image." << std::endl;

    try
    {
        if(!OpenClipboard(nullptr))
        {
            if(ERROR_ACCESS_DENIED != GetLastError())
            {
                throw kernel_failure("OpenClipboard()");
            }

            std::cout << "Clipboard is locked by another application." << std::endl;

            bool success = false;
            for(auto i = 1; i < 6; i++)
            {
                std::cout << "Waiting for clipboard. Attempt " << i << "/5." << std::endl;
                Sleep(5);

                if(OpenClipboard(nullptr))
                {
                    success = true;
                    break;
                }

                if(ERROR_ACCESS_DENIED != GetLastError())
                {
                    throw kernel_failure("OpenClipboard()");
                }
            }

            if(!success)
            {
                throw kernel_failure("OpenClipboard()");
            }
        }

        try
        {
            auto clipboard_data = GetClipboardData(CF_BITMAP);
            if(nullptr == clipboard_data)
            {
                throw kernel_failure("GetClipboardData()");
            }

            HRESULT result;
            Gdiplus::Bitmap gdi_bitmap((HBITMAP) clipboard_data, nullptr);

            IStream * is = nullptr;
            result = CreateStreamOnHGlobal(nullptr, TRUE, &is);
            if(S_OK != result)
            {
                throw hresult_failure("CreateStreamOnHGlobal()", result);
            }

            CLSID clsid_png;
            result = CLSIDFromString(L"{557cf406-1a04-11d3-9a73-0000f81ef32e}", &clsid_png);
            if(S_OK != result)
            {
                throw hresult_failure("CLSIDFromString()", result);
            }

            Gdiplus::Status status = gdi_bitmap.Save(is, &clsid_png);
            if(Gdiplus::Status::Ok != status)
            {
                throw gdi_failure("Gdiplus::Bitmap::Save()", status);
            }

            HGLOBAL hg = nullptr;
            result = GetHGlobalFromStream(is, &hg);
            if(S_OK != result)
            {
                throw hresult_failure("CLSIDFromString()", result);
            }

            auto buffer_size = GlobalSize(hg);
            new_image_data.resize(buffer_size);

            LPVOID image_ptr = GlobalLock(hg);
            memcpy(&new_image_data[0], image_ptr, buffer_size);
            GlobalUnlock(hg);

            is->Release();
            CloseClipboard();
        }
        catch(...)
        {
            CloseClipboard();
            std::rethrow_exception(std::current_exception());
        }

        if(old_image_data == new_image_data)
        {
            std::cout << "New image is equal to old image." << std::endl;
            return DefWindowProc(handle, message, wp, lp);
        }

        std::string filename = std::to_string(get_timestamp()) + ".png";
        std::ofstream os(save_path + filename, std::ios::binary);
        os.write((char *) new_image_data.data(), new_image_data.size());
        old_image_data = new_image_data;

        std::cout << "Image is saved as " << filename << "." << std::endl;
    }
    catch(const std::exception & exception)
    {
        std::cerr << "Unable to process clipboard:" << std::endl;
        std::cerr << exception.what() << std::endl;
    }

    return DefWindowProc(handle, message, wp, lp);
}

int main(int arguments_size, char ** arguments)
{
    ULONG_PTR gdiplusToken;

    save_path = 2 <= arguments_size ? arguments[1] : "";

    try
    {
        Gdiplus::GdiplusStartupInput gdiplusStartupInput;
        Gdiplus::Status status = Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr);
        if(Gdiplus::Status::Ok != status)
        {
            throw gdi_failure("GdiplusStartup()", status);
        }

        WNDCLASSEX window_class = {0};
        window_class.cbSize = sizeof(WNDCLASSEX);
        window_class.lpfnWndProc = window_procedure;
        window_class.hInstance = GetModuleHandle(nullptr);
        window_class.lpszClassName = class_name;

        if(0 == RegisterClassEx(&window_class))
        {
            throw kernel_failure("RegisterClassEx()");
        }

        auto handle = CreateWindowEx(0, class_name, "CIAS_NAME", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, nullptr, nullptr);

        if(nullptr == handle)
        {
            throw kernel_failure("CreateWindowEx()");
        }

        if(FALSE == AddClipboardFormatListener(handle))
        {
            throw kernel_failure("AddClipboardFormatListener()");
        }
    }
    catch(const std::exception & exception)
    {
        std::cerr << "Unable to start:" << std::endl;
        std::cerr << exception.what() << std::endl;

        system("pause");

        return 1;
    }

    std::cout << "Work is established." << std::endl;

    MSG message = {0};
    while(0 != GetMessage(&message, nullptr, 0, 0))
    {
        TranslateMessage(&message);
        DispatchMessage(&message);
    }

    Gdiplus::GdiplusShutdown(gdiplusToken);

    return 0;
}