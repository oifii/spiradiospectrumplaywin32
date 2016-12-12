// Copyright (c) 2016 stephane.poirier@oifii.org
// 2016march26, created by stephane.poirier@oifii.org (or spi@oifii.org)
//
// spiradiospectrumplaywin32.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include "spiradiospectrumplaywin32.h"
#include "FreeImage.h"
#include <shellapi.h> //for CommandLineToArgW()
#include <mmsystem.h> //for timeSetEvent()
#include <stdio.h> //for swprintf()
#include <assert.h>
#include "spiwavsetlib.h"
#include "bass.h"
#include <Shlwapi.h>

//spi, begin, code from spispectrumplay_xaos
#include <process.h>    // _beginthread, _endthread 
#include <math.h>
#include <malloc.h>
int SPECWIDTH=-1;	// display width
int SPECHEIGHT=-1;	// display height 
char global_buffer[1024];
HDC specdc=0;
HBITMAP specbmp=0;
BYTE* specbuf;
int specmode=3,specpos=0; // spectrum mode (and marker pos for 2nd mode)
int global_idcolorpalette=0;
int global_bands=20;
DWORD global_timer_updatespectrum=0;
int global_skip_updatespectrum=0;
DWORD global_hwnd_timerid_prebuffermonitoring=0;
DWORD global_hwnd_timerid_skipupdatespectrum=1;
//spi, end, code from spispectrumplay_xaos

// Global Variables:

#define MAX_LOADSTRING 100
FIBITMAP* global_dib;
HFONT global_hFont;
HWND global_hwnd=NULL;
MMRESULT global_timer=0;
//#define MAX_GLOBALTEXT	4096
//WCHAR global_text[MAX_GLOBALTEXT+1];
string global_filename="radiostations.txt";
float global_duration_sec=180.0;
float global_sleeptimeperstation_sec=30.0;
int global_x=100;
int global_y=200;
int global_xwidth=400;
int global_yheight=400;
BYTE global_alpha=200;
int global_fontheight=24;
int global_fontwidth=-1; //will be computed within WM_PAINT handler
int global_staticalignment = SS_LEFT; //0 for left, 1 for center and 2 for right (SS_LEFT, SS_CENTER or SS_RIGHT)
int global_staticheight=-1; //will be computed within WM_SIZE handler
int global_staticwidth=-1; //will be computed within WM_SIZE handler 
//COLORREF global_statictextcolor=RGB(0xFF, 0xFF, 0xFF); //white
//COLORREF global_statictextcolor=RGB(0xFF, 0x00, 0x00); //red
COLORREF global_statictextcolor=RGB(0x00, 0x00, 0xFF); //blue
//spi, begin
int global_imageheight=-1; //will be computed within WM_SIZE handler
int global_imagewidth=-1; //will be computed within WM_SIZE handler 
//spi, end
int global_titlebardisplay=1; //0 for off, 1 for on
int global_acceleratoractive=0; //0 for off, 1 for on
int global_menubardisplay=0; //0 for off, 1 for on

DWORD global_startstamp_ms;
//FILE* global_pFILE=NULL;
string global_line;
std::ifstream global_ifstream;
char global_url[1024];

char global_proxy[100]=""; // proxy server
CRITICAL_SECTION global_lock;
DWORD global_req=0;	// request number/counter
HSTREAM global_chan;	// stream handle


#define IDC_MAIN_EDIT	100
#define IDC_MAIN_STATIC	101

HINSTANCE hInst;								// current instance
//TCHAR szTitle[MAX_LOADSTRING];					// The title bar text
//TCHAR szWindowClass[MAX_LOADSTRING];			// the main window class name
TCHAR szTitle[1024]={L"spiradiospectrumplaywin32title"};					// The title bar text
TCHAR szWindowClass[1024]={L"spiradiospectrumplaywin32class"};			// the main window class name

//new parameters
string global_begin="begin.ahk";
string global_end="end.ahk";

// Forward declarations of functions included in this code module:
ATOM				MyRegisterClass(HINSTANCE hInstance);
BOOL				InitInstance(HINSTANCE, int);
LRESULT CALLBACK	WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK	About(HWND, UINT, WPARAM, LPARAM);

//spi, begin, code from spispectrumplay_xaos
void CALLBACK UpdateSpectrum(UINT uTimerID, UINT uMsg, DWORD dwUser, DWORD dw1, DWORD dw2)
{
	HDC dc;
	int x,y,y1;

	if(global_skip_updatespectrum) return;

	DWORD myDWORD = BASS_ChannelIsActive(global_chan);
	if(myDWORD==BASS_ACTIVE_PLAYING)
	{
		if (specmode==3) 
		{ // waveform
			int c;
			float *buf;
			BASS_CHANNELINFO ci;
			memset(specbuf,0,SPECWIDTH*SPECHEIGHT);
			BASS_ChannelGetInfo(global_chan,&ci); // get number of channels
			//buf=alloca(ci.chans*SPECWIDTH*sizeof(float)); // allocate buffer for data
			buf=(float*)alloca(ci.chans*SPECWIDTH*sizeof(float)); // allocate buffer for data
			BASS_ChannelGetData(global_chan,buf,(ci.chans*SPECWIDTH*sizeof(float))|BASS_DATA_FLOAT); // get the sample data (floating-point to avoid 8 & 16 bit processing)
			for (c=0;c<ci.chans;c++) 
			{
				for (x=0;x<SPECWIDTH;x++) 
				{
					int v=(1-buf[x*ci.chans+c])*SPECHEIGHT/2; // invert and scale to fit display
					if (v<0) v=0;
					else if (v>=SPECHEIGHT) v=SPECHEIGHT-1;
					if (!x) y=v;
					do 
					{ // draw line from previous sample...
						if (y<v) y++;
						else if (y>v) y--;
						specbuf[y*SPECWIDTH+x]=c&1?127:1; // left=green, right=red (could add more colours to palette for more chans)
					} while (y!=v);
				}
			}
		} 
		else 
		{
			/*
			float fft[1024];
			BASS_ChannelGetData(global_chan,fft,BASS_DATA_FFT2048); // get the FFT data, BASS_DATA_FFT2048 returns 1024 values
			*/
			float fft[2048];
			if(SPECWIDTH<2048)
			{
				BASS_ChannelGetData(global_chan,fft,BASS_DATA_FFT2048); // get the FFT data, BASS_DATA_FFT2048 returns 1024 values
			}
			else if(SPECWIDTH<4096)
			{
				BASS_ChannelGetData(global_chan,fft,BASS_DATA_FFT4096); // get the FFT data, BASS_DATA_FFT4096 returns 2048 values
			}
			if (!specmode) 
			{ // "normal" FFT
				memset(specbuf,0,SPECWIDTH*SPECHEIGHT);
				for (x=0;x<SPECWIDTH/2;x++) 
				{
	#if 1
					y=sqrt(fft[x+1])*3*SPECHEIGHT-4; // scale it (sqrt to make low values more visible)
	#else
					y=fft[x+1]*10*SPECHEIGHT; // scale it (linearly)
	#endif
					if (y>SPECHEIGHT) y=SPECHEIGHT; // cap it
					if (x && (y1=(y+y1)/2)) // interpolate from previous to make the display smoother
						//while (--y1>=0) specbuf[y1*SPECWIDTH+x*2-1]=y1+1;
						while (--y1>=0) specbuf[y1*SPECWIDTH+x*2-1]=(127*y1/SPECHEIGHT)+1;
					y1=y;
					//while (--y>=0) specbuf[y*SPECWIDTH+x*2]=y+1; // draw level
					while (--y>=0) specbuf[y*SPECWIDTH+x*2]=(127*y/SPECHEIGHT)+1; // draw level
				}
			} 
			else if (specmode==1) 
			{ // logarithmic, acumulate & average bins
				int b0=0;
				memset(specbuf,0,SPECWIDTH*SPECHEIGHT);
	//#define BANDS 28
	//#define BANDS 80
	//#define BANDS 12
				for (x=0;x<global_bands;x++) 
				{
					float peak=0;
					int b1=pow(2,x*10.0/(global_bands-1));
					if (b1>1023) b1=1023;
					if (b1<=b0) b1=b0+1; // make sure it uses at least 1 FFT bin
					for (;b0<b1;b0++)
						if (peak<fft[1+b0]) peak=fft[1+b0];
					y=sqrt(peak)*3*SPECHEIGHT-4; // scale it (sqrt to make low values more visible)
					if (y>SPECHEIGHT) y=SPECHEIGHT; // cap it
					while (--y>=0)
						//memset(specbuf+y*SPECWIDTH+x*(SPECWIDTH/global_bands),y+1,SPECWIDTH/global_bands-2); // draw bar
						memset(specbuf+y*SPECWIDTH+x*(SPECWIDTH/global_bands),(127*y/SPECHEIGHT)+1,SPECWIDTH/global_bands-2); // draw bar
				}
			} 
			else 
			{ // "3D"
				for (x=0;x<SPECHEIGHT;x++) 
				{
					y=sqrt(fft[x+1])*3*127; // scale it (sqrt to make low values more visible)
					if (y>127) y=127; // cap it
					specbuf[x*SPECWIDTH+specpos]=128+y; // plot it
				}
				// move marker onto next position
				specpos=(specpos+1)%SPECWIDTH;
				for (x=0;x<SPECHEIGHT;x++) specbuf[x*SPECWIDTH+specpos]=255;
			}
		}

		// update the display
		dc=GetDC(global_hwnd);
		BitBlt(dc,0,0,SPECWIDTH,SPECHEIGHT,specdc,0,0,SRCCOPY);
		ReleaseDC(global_hwnd,dc);
	}
}
//spi, end, code from spispectrumplay_xaos

// Convert a wide Unicode string to an UTF8 string
std::string utf8_encode(const std::wstring &wstr)
{
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string strTo( size_needed, 0 );
    WideCharToMultiByte                  (CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
    return strTo;
}

// Convert an UTF8 string to a wide Unicode String
std::wstring utf8_decode(const std::string &str)
{
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
    std::wstring wstrTo( size_needed, 0 );
    MultiByteToWideChar                  (CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
    return wstrTo;
}

// update stream title from metadata
void DoMeta()
{
	const char *meta=BASS_ChannelGetTags(global_chan,BASS_TAG_META);
	if (meta) 
	{ 
		// got Shoutcast metadata
		const char *p=strstr(meta,"StreamTitle='"); // locate the title
		if (p) 
		{
			const char *p2=strstr(p,"';"); // locate the end of it
			if (p2) 
			{
				char *t=strdup(p+13);
				t[p2-(p+13)]=0;
				//MESS(30,WM_SETTEXT,0,t);
				StatusAddText(t);
				StatusAddText("\n");
				free(t);
			}
		}
	} 
	else 
	{
		meta=BASS_ChannelGetTags(global_chan,BASS_TAG_OGG);
		if (meta) 
		{ 
			// got Icecast/OGG tags
			const char *artist=NULL,*title=NULL,*p=meta;
			for (;*p;p+=strlen(p)+1) 
			{
				if (!strnicmp(p,"artist=",7)) // found the artist
					artist=p+7;
				if (!strnicmp(p,"title=",6)) // found the title
					title=p+6;
			}
			if (title) 
			{
				if (artist) 
				{
					char text[100];
					_snprintf(text,sizeof(text),"%s - %s",artist,title);
					//MESS(30,WM_SETTEXT,0,text);
					StatusAddText(text);
					StatusAddText("\n");

				} 
				else
				{
					//MESS(30,WM_SETTEXT,0,title);
					StatusAddText(title);
					StatusAddText("\n");
				}
			}
		}
	}
}

void CALLBACK MetaSync(HSYNC handle, DWORD channel, DWORD data, void *user)
{
	DoMeta();
}

void CALLBACK EndSync(HSYNC handle, DWORD channel, DWORD data, void *user)
{
	/*
	MESS(31,WM_SETTEXT,0,"not playing");
	MESS(30,WM_SETTEXT,0,"");
	MESS(32,WM_SETTEXT,0,"");
	*/
	StatusAddText("not playing\n");

}

void CALLBACK StatusProc(const void *buffer, DWORD length, void *user)
{
	if (buffer && !length && (DWORD)user==global_req) 
	{
		// got HTTP/ICY tags, and this is still the current request
		/*
		MESS(32,WM_SETTEXT,0,buffer); // display status
		*/
		StatusAddText((const char*)buffer);
		StatusAddText("\n");
	}
}

void __cdecl OpenURL(char *url)
{
	DWORD c,r;
EnterCriticalSection(&global_lock); // make sure only 1 thread at a time can do the following
	r=++global_req; // increment the request counter for this request
LeaveCriticalSection(&global_lock);
	//KillTimer(global_hwnd,0); // stop prebuffer monitoring
	KillTimer(global_hwnd, global_hwnd_timerid_prebuffermonitoring); // stop prebuffer monitoring
	BASS_StreamFree(global_chan); // close old stream
	/*
	MESS(31,WM_SETTEXT,0,"connecting...");
	MESS(30,WM_SETTEXT,0,"");
	MESS(32,WM_SETTEXT,0,"");
	*/
	StatusAddText("connecting...\n");
	c=BASS_StreamCreateURL(url,0,BASS_STREAM_BLOCK|BASS_STREAM_STATUS|BASS_STREAM_AUTOFREE,StatusProc,(void*)r); // open URL
	free(url); // free temp URL buffer
EnterCriticalSection(&global_lock);
	if (r!=global_req) 
	{ 
		// there is a newer request, discard this stream
LeaveCriticalSection(&global_lock);
		if (c) BASS_StreamFree(c);
		return;
	}
	global_chan=c; // this is now the current stream
LeaveCriticalSection(&global_lock);
	if (!global_chan) 
	{ 
		// failed to open
		/*
		MESS(31,WM_SETTEXT,0,"not playing");
		Error("Can't play the stream");
		*/
	} 
	else
	{
		//SetTimer(global_hwnd,0,5,0); // start prebuffer monitoring
		SetTimer(global_hwnd, global_hwnd_timerid_prebuffermonitoring,5,0); // start prebuffer monitoring
	}
}

/*
void CALLBACK StartGlobalProcess(UINT uTimerID, UINT uMsg, DWORD dwUser, DWORD dw1, DWORD dw2)
{
	WavSetLib_Initialize(global_hwnd, IDC_MAIN_STATIC, global_staticwidth, global_staticheight, global_fontwidth, global_fontheight, global_staticalignment);

	string displayline="";
	wstring url=L"";

	DWORD nowstamp_ms = GetTickCount();
	while( (global_duration_sec<0.0f) || ((nowstamp_ms-global_startstamp_ms)/1000.0f)<global_duration_sec )
	{		
		if(global_ifstream.eof()) 
		{
			global_ifstream.close();
			global_ifstream.open(global_filename.c_str(), ios_base::in);
			//global_line="";
		}
		std::getline(global_ifstream, global_line);

		if(global_line!="")
		{
			url = utf8_decode(global_line);
			if(PathIsURL(url.c_str())) 
			{
				//global_line is a valid url
				displayline = global_line + "\n";
				StatusAddText(displayline.c_str());
				//open url

// open URL in a new thread (so that main thread is free)
//_beginthread(OpenURL,0,url);
//_beginthread((void(__cdecl*)(void*))OpenURL,0,url);

				//strcpy(global_url, global_line.c_str());
				//OpenURL(global_url);
				char* local_url=strdup(global_line.c_str());
				OpenURL(local_url);
// open URL in a new thread (so that main thread is free)
//_beginthread((void(__cdecl*)(void*))OpenURL,0,local_url);

				//sleep on radio station
				Sleep((int)(global_sleeptimeperstation_sec*1000));
			}
		}
		nowstamp_ms = GetTickCount();
	}
	PostMessage(global_hwnd, WM_DESTROY, 0, 0);
}
*/

void __cdecl StartGlobalProcess(void)
{
	Sleep(500);
	WavSetLib_Initialize(global_hwnd, IDC_MAIN_STATIC, global_staticwidth, global_staticheight, global_fontwidth, global_fontheight, global_staticalignment);

	string displayline="";
	wstring url=L"";

	DWORD nowstamp_ms = GetTickCount();
	while( (global_duration_sec<0.0f) || ((nowstamp_ms-global_startstamp_ms)/1000.0f)<global_duration_sec )
	{		
		if(global_ifstream.eof()) 
		{
			global_ifstream.close();
			global_ifstream.open(global_filename.c_str(), ios_base::in);
			//global_line="";
		}
		std::getline(global_ifstream, global_line);

		if(global_line!="")
		{
			url = utf8_decode(global_line);
			if(PathIsURL(url.c_str())) 
			{
				//global_line is a valid url
				displayline = global_line + "\n";
				StatusAddText(displayline.c_str());
				//open url

// open URL in a new thread (so that main thread is free)
//_beginthread(OpenURL,0,url);
//_beginthread((void(__cdecl*)(void*))OpenURL,0,url);

				//strcpy(global_url, global_line.c_str());
				//OpenURL(global_url);
				char* local_url=strdup(global_line.c_str());
				OpenURL(local_url);
// open URL in a new thread (so that main thread is free)
//_beginthread((void(__cdecl*)(void*))OpenURL,0,local_url);

				//sleep on radio station
				Sleep((int)(global_sleeptimeperstation_sec*1000));
			}
		}
		nowstamp_ms = GetTickCount();
	}
	PostMessage(global_hwnd, WM_DESTROY, 0, 0);
}



PCHAR*
    CommandLineToArgvA(
        PCHAR CmdLine,
        int* _argc
        )
    {
        PCHAR* argv;
        PCHAR  _argv;
        ULONG   len;
        ULONG   argc;
        CHAR   a;
        ULONG   i, j;

        BOOLEAN  in_QM;
        BOOLEAN  in_TEXT;
        BOOLEAN  in_SPACE;

        len = strlen(CmdLine);
        i = ((len+2)/2)*sizeof(PVOID) + sizeof(PVOID);

        argv = (PCHAR*)GlobalAlloc(GMEM_FIXED,
            i + (len+2)*sizeof(CHAR));

        _argv = (PCHAR)(((PUCHAR)argv)+i);

        argc = 0;
        argv[argc] = _argv;
        in_QM = FALSE;
        in_TEXT = FALSE;
        in_SPACE = TRUE;
        i = 0;
        j = 0;

        while( a = CmdLine[i] ) {
            if(in_QM) {
                if(a == '\"') {
                    in_QM = FALSE;
                } else {
                    _argv[j] = a;
                    j++;
                }
            } else {
                switch(a) {
                case '\"':
                    in_QM = TRUE;
                    in_TEXT = TRUE;
                    if(in_SPACE) {
                        argv[argc] = _argv+j;
                        argc++;
                    }
                    in_SPACE = FALSE;
                    break;
                case ' ':
                case '\t':
                case '\n':
                case '\r':
                    if(in_TEXT) {
                        _argv[j] = '\0';
                        j++;
                    }
                    in_TEXT = FALSE;
                    in_SPACE = TRUE;
                    break;
                default:
                    in_TEXT = TRUE;
                    if(in_SPACE) {
                        argv[argc] = _argv+j;
                        argc++;
                    }
                    _argv[j] = a;
                    j++;
                    in_SPACE = FALSE;
                    break;
                }
            }
            i++;
        }
        _argv[j] = '\0';
        argv[argc] = NULL;

        (*_argc) = argc;
        return argv;
    }

int APIENTRY _tWinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPTSTR    lpCmdLine,
                     int       nCmdShow)
{

	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	global_startstamp_ms = GetTickCount();

	//LPWSTR *szArgList;
	LPSTR *szArgList;
	int nArgs;
	int i;

	//szArgList = CommandLineToArgvW(GetCommandLineW(), &nArgs);
	szArgList = CommandLineToArgvA(GetCommandLineA(), &nArgs);
	if( NULL == szArgList )
	{
		//wprintf(L"CommandLineToArgvW failed\n");
		return FALSE;
	}
	LPWSTR *szArgListW;
	int nArgsW;
	szArgListW = CommandLineToArgvW(GetCommandLineW(), &nArgsW);
	if( NULL == szArgListW )
	{
		//wprintf(L"CommandLineToArgvW failed\n");
		return FALSE;
	}

	if(nArgs>1)
	{
		global_filename = szArgList[1];
	}
	if(nArgs>2)
	{
		global_duration_sec = atof(szArgList[2]);
	}
	if(nArgs>3)
	{
		global_sleeptimeperstation_sec = atof(szArgList[3]);
	}
	if(nArgs>4)
	{
		global_x = atoi(szArgList[4]);
	}
	if(nArgs>5)
	{
		global_y = atoi(szArgList[5]);
	}
	if(nArgs>6)
	{
		global_xwidth = atoi(szArgList[6]);
	}
	if(nArgs>7)
	{
		global_yheight = atoi(szArgList[7]);
	}
	if(nArgs>8)
	{
		global_alpha = atoi(szArgList[8]);
	}
	if(nArgs>9)
	{
		global_titlebardisplay = atoi(szArgList[9]);
	}
	if(nArgs>10)
	{
		global_menubardisplay = atoi(szArgList[10]);
	}
	if(nArgs>11)
	{
		global_acceleratoractive = atoi(szArgList[11]);
	}
	if(nArgs>12)
	{
		global_fontheight = atoi(szArgList[12]);
	}
	//new parameters
	if(nArgs>13)
	{
		wcscpy(szWindowClass, szArgListW[13]); 
	}
	if(nArgs>14)
	{
		wcscpy(szTitle, szArgListW[14]); 
	}
	if(nArgs>15)
	{
		global_begin = szArgList[15]; 
	}
	if(nArgs>16)
	{
		global_end = szArgList[16]; 
	}
	if(nArgs>17)
	{
		specmode = atoi((LPCSTR)(szArgList[17]));
	}
	if(nArgs>18)
	{
		global_idcolorpalette = atoi((LPCSTR)(szArgList[18]));
	}
	if(nArgs>19)
	{
		global_bands = atoi((LPCSTR)(szArgList[19]));
	}
	LocalFree(szArgList);
 	LocalFree(szArgListW);
	
	int nShowCmd = false;
	//ShellExecuteA(NULL, "open", "begin.bat", "", NULL, nShowCmd);
	ShellExecuteA(NULL, "open", global_begin.c_str(), "", NULL, nCmdShow);

	MSG msg;
	HACCEL hAccelTable;

	// Initialize global strings
	//LoadString(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	//LoadString(hInstance, IDC_SPIWAVWIN32, szWindowClass, MAX_LOADSTRING);
	MyRegisterClass(hInstance);

	// Perform application initialization:
	if (!InitInstance (hInstance, nCmdShow))
	{
		return FALSE;
	}

	if(global_acceleratoractive)
	{
		hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_SPIWAVWIN32));
	}
	else
	{
		hAccelTable = NULL;
	}
	// Main message loop:
	while (GetMessage(&msg, NULL, 0, 0))
	{
		if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	return (int) msg.wParam;
}



//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
//  COMMENTS:
//
//    This function and its usage are only necessary if you want this code
//    to be compatible with Win32 systems prior to the 'RegisterClassEx'
//    function that was added to Windows 95. It is important to call this function
//    so that the application will get 'well formed' small icons associated
//    with it.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEX wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style			= CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc	= WndProc;
	wcex.cbClsExtra		= 0;
	wcex.cbWndExtra		= 0;
	wcex.hInstance		= hInstance;
	//wcex.hIcon			= LoadIcon(hInstance, MAKEINTRESOURCE(IDI_SPIWAVWIN32));
	wcex.hIcon			= (HICON)LoadImage(NULL, L"background_32x32x16.ico", IMAGE_ICON, 0, 0, LR_LOADFROMFILE);
	wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground	= (HBRUSH)(COLOR_WINDOW+1);

	if(global_menubardisplay)
	{
		wcex.lpszMenuName = MAKEINTRESOURCE(IDC_SPIWAVWIN32); //original with menu
	}
	else
	{
		wcex.lpszMenuName = NULL; //no menu
	}
	wcex.lpszClassName	= szWindowClass;
	//wcex.hIconSm		= LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));
	wcex.hIconSm		= (HICON)LoadImage(NULL, L"background_16x16x16.ico", IMAGE_ICON, 0, 0, LR_LOADFROMFILE);

	return RegisterClassEx(&wcex);
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
	HWND hWnd;

	hInst = hInstance; // Store instance handle in our global variable

	global_dib = FreeImage_Load(FIF_JPEG, "background.jpg", JPEG_DEFAULT);

	//global_hFont=CreateFontW(global_fontheight,0,0,0,FW_NORMAL,0,0,0,0,0,0,2,0,L"SYSTEM_FIXED_FONT");
	//global_hFont=CreateFontW(global_fontheight,0,0,0,FW_BOLD,0,0,0,0,0,0,2,0,L"Segoe Script");
	global_hFont=CreateFontW(global_fontheight,0,0,0,FW_EXTRABOLD,0,0,0,0,0,0,2,0,L"Segoe Script");

	if(global_titlebardisplay)
	{
		hWnd = CreateWindow(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW, //original with WS_CAPTION etc.
			global_x, global_y, global_xwidth, global_yheight, NULL, NULL, hInstance, NULL);
	}
	else
	{
		hWnd = CreateWindow(szWindowClass, szTitle, WS_POPUP | WS_VISIBLE, //no WS_CAPTION etc.
			global_x, global_y, global_xwidth, global_yheight, NULL, NULL, hInstance, NULL);
	}
	if (!hWnd)
	{
		return FALSE;
	}
	global_hwnd = hWnd;

	SetWindowLong(hWnd, GWL_EXSTYLE, GetWindowLong(hWnd, GWL_EXSTYLE) | WS_EX_LAYERED);
	SetLayeredWindowAttributes(hWnd, 0, global_alpha, LWA_ALPHA);

	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);

	// initialize default output device
	if (!BASS_Init(-1,44100,0,hWnd,NULL)) 
	{
		//Error("Can't initialize device");
		//DestroyWindow(win);
		return FALSE;
	}
	BASS_SetConfig(BASS_CONFIG_NET_PLAYLIST,1); // enable playlist processing
	BASS_SetConfig(BASS_CONFIG_NET_PREBUF,0); // minimize automatic pre-buffering, so we can do it (and display it) instead
	BASS_SetConfigPtr(BASS_CONFIG_NET_PROXY,global_proxy); // setup proxy server location
	InitializeCriticalSection(&global_lock);

	//global_timer=timeSetEvent(500,1000,(LPTIMECALLBACK)&StartGlobalProcess,0,TIME_ONESHOT); //can't have two timeSetEvent() call
	//so, call StartGlobalProcess() in a new thread (so that main thread is free)
	_beginthread((void(__cdecl*)(void*))StartGlobalProcess,0,0);

	//spi, begin, code from spispectrumplay_xaos
	//setup update timer (40hz)
	global_timer_updatespectrum=timeSetEvent(25,25,(LPTIMECALLBACK)&UpdateSpectrum,0,TIME_PERIODIC);
	//spi, end, code from spispectrumplay_xaos
	return TRUE;
}

//spi, begin, code from spispectrumplay_xaos
void CreateBitmapToDrawSpectrum()
{
	global_skip_updatespectrum=1;

	//let current updatespectrum finish before reallocating
	Sleep(25);
	//if(global_hwnd) InvalidateRect(global_hwnd, NULL, false);

	//free previously allocated memory
	//so function can be called OnSize()
	if (specdc) DeleteDC(specdc);
	if (specbmp) DeleteObject(specbmp);

	// create bitmap to draw spectrum in (8 bit for easy updating)
	BYTE data[2000]={0};
	BITMAPINFOHEADER *bh=(BITMAPINFOHEADER*)data;
	RGBQUAD *pal=(RGBQUAD*)(data+sizeof(*bh));
	int a;
	bh->biSize=sizeof(*bh);
	bh->biWidth=SPECWIDTH;
	bh->biHeight=SPECHEIGHT; // upside down (line 0=bottom)
	bh->biPlanes=1;
	bh->biBitCount=8;
	bh->biClrUsed=bh->biClrImportant=256;
	// setup palette
				
	if(global_idcolorpalette==0)
	{
		//original palette, green shifting to red
		for (a=1;a<128;a++) {
			pal[a].rgbGreen=256-2*a;
			pal[a].rgbRed=2*a;
		}
		for (a=0;a<32;a++) {
			pal[128+a].rgbBlue=8*a;
			pal[128+32+a].rgbBlue=255;
			pal[128+32+a].rgbRed=8*a;
			pal[128+64+a].rgbRed=255;
			pal[128+64+a].rgbBlue=8*(31-a);
			pal[128+64+a].rgbGreen=8*a;
			pal[128+96+a].rgbRed=255;
			pal[128+96+a].rgbGreen=255;
			pal[128+96+a].rgbBlue=8*a;
		}
	}
	else if(global_idcolorpalette==1)
	{
		//altered palette, red shifting to green
		for (a=1;a<128;a++) {
			pal[a].rgbRed=256-2*a;
			pal[a].rgbGreen=2*a;
		}
		for (a=0;a<32;a++) {
			pal[128+a].rgbBlue=8*a;
			pal[128+32+a].rgbBlue=255;
			pal[128+32+a].rgbGreen=8*a;
			pal[128+64+a].rgbGreen=255;
			pal[128+64+a].rgbBlue=8*(31-a);
			pal[128+64+a].rgbRed=8*a;
			pal[128+96+a].rgbGreen=255;
			pal[128+96+a].rgbRed=255;
			pal[128+96+a].rgbBlue=8*a;
		}
	}
	else if(global_idcolorpalette==2)
	{
		//altered palette, blue shifting to green
		for (a=1;a<128;a++) {
			pal[a].rgbBlue=256-2*a;
			pal[a].rgbGreen=2*a;
		}
		for (a=0;a<32;a++) {
			pal[128+a].rgbBlue=8*a;
			pal[128+32+a].rgbRed=255;
			pal[128+32+a].rgbGreen=8*a;
			pal[128+64+a].rgbGreen=255;
			pal[128+64+a].rgbRed=8*(31-a);
			pal[128+64+a].rgbBlue=8*a;
			pal[128+96+a].rgbGreen=255;
			pal[128+96+a].rgbBlue=255;
			pal[128+96+a].rgbRed=8*a;
		}
	}
	else if(global_idcolorpalette==3)
	{
		//altered palette, black shifting to white - grascale
		for (a=1;a<256;a++) {
			pal[a].rgbRed=a;
			pal[a].rgbBlue=a;
			pal[a].rgbGreen=a;
		}
	}
	// create the bitmap
	specbmp=CreateDIBSection(0,(BITMAPINFO*)bh,DIB_RGB_COLORS,(void**)&specbuf,NULL,0);
	specdc=CreateCompatibleDC(0);
	SelectObject(specdc,specbmp);

	//global_skip_updatespectrum=0;
	//avoid releasing skip update spectrum to quick
	KillTimer(global_hwnd, global_hwnd_timerid_skipupdatespectrum);
	SetTimer(global_hwnd, global_hwnd_timerid_skipupdatespectrum,5000,0); 
	return;
}
//spi, end, code from spispectrumplay_xaos

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE:  Processes messages for the main window.
//
//  WM_COMMAND	- process the application menu
//  WM_PAINT	- Paint the main window
//  WM_DESTROY	- post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	int wmId, wmEvent;
	PAINTSTRUCT ps;
	HDC hdc;
	HGDIOBJ hOldBrush;
	HGDIOBJ hOldPen;
	int iOldMixMode;
	COLORREF crOldBkColor;
	COLORREF crOldTextColor;
	int iOldBkMode;
	HFONT hOldFont, hFont;
	TEXTMETRIC myTEXTMETRIC;

	switch (message)
	{
	case WM_CREATE:
		{
			/*
			HWND hStatic = CreateWindowEx(WS_EX_TRANSPARENT, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_CENTER,  
				0, 100, 100, 100, hWnd, (HMENU)IDC_MAIN_STATIC, GetModuleHandle(NULL), NULL);
			*/
			HWND hStatic = CreateWindowEx(WS_EX_TRANSPARENT, L"STATIC", L"", WS_CHILD | WS_VISIBLE | global_staticalignment,  
				0, 100, 100, 100, hWnd, (HMENU)IDC_MAIN_STATIC, GetModuleHandle(NULL), NULL);
			if(hStatic == NULL)
				MessageBox(hWnd, L"Could not create static text.", L"Error", MB_OK | MB_ICONERROR);
			SendMessage(hStatic, WM_SETFONT, (WPARAM)global_hFont, MAKELPARAM(FALSE, 0));

			//spi, begin, code from spispectrumplay_xaos
			SPECWIDTH=global_xwidth-global_xwidth%4;	
			SPECHEIGHT=global_yheight; 
			CreateBitmapToDrawSpectrum();
			//spi, end, code from spispectrumplay_xaos
		}
		break;
	case WM_SIZE:
		{
			RECT rcClient;

			GetClientRect(hWnd, &rcClient);
			/*
			HWND hEdit = GetDlgItem(hWnd, IDC_MAIN_EDIT);
			SetWindowPos(hEdit, NULL, 0, 0, rcClient.right/2, rcClient.bottom/2, SWP_NOZORDER);
			*/
			HWND hStatic = GetDlgItem(hWnd, IDC_MAIN_STATIC);
			global_staticwidth = rcClient.right - 0;
			//global_staticheight = rcClient.bottom-(rcClient.bottom/2);
			global_staticheight = rcClient.bottom-0;
			//spi, begin
			global_imagewidth = rcClient.right - 0;
			global_imageheight = rcClient.bottom - 0; 
			WavSetLib_Initialize(global_hwnd, IDC_MAIN_STATIC, global_staticwidth, global_staticheight, global_fontwidth, global_fontheight, global_staticalignment);
			//spi, end
			//SetWindowPos(hStatic, NULL, 0, rcClient.bottom/2, global_staticwidth, global_staticheight, SWP_NOZORDER);
			SetWindowPos(hStatic, NULL, 0, 0, global_staticwidth, global_staticheight, SWP_NOZORDER);

			//spi, begin, code from spispectrumplay_xaos
			SPECWIDTH=global_imagewidth-global_imagewidth%4;	
			SPECHEIGHT=global_imageheight; 
			CreateBitmapToDrawSpectrum();
			//spi, begin, code from spispectrumplay_xaos

		}
		break;
	case WM_CTLCOLOREDIT:
		{
			SetBkMode((HDC)wParam, TRANSPARENT);
			SetTextColor((HDC)wParam, RGB(0xFF, 0xFF, 0xFF));
			return (INT_PTR)::GetStockObject(NULL_PEN);
		}
		break;
	case WM_CTLCOLORSTATIC:
		{
			SetBkMode((HDC)wParam, TRANSPARENT);
			//SetTextColor((HDC)wParam, RGB(0xFF, 0xFF, 0xFF)); 
			SetTextColor((HDC)wParam, global_statictextcolor); 
			return (INT_PTR)::GetStockObject(NULL_PEN);
		}
		break;
	case WM_TIMER:
		{ 
			if(wParam==global_hwnd_timerid_prebuffermonitoring)
			{
				// monitor prebuffering progress
				DWORD progress=BASS_StreamGetFilePosition(global_chan,BASS_FILEPOS_BUFFER)
					*100/BASS_StreamGetFilePosition(global_chan,BASS_FILEPOS_END); // percentage of buffer filled
				if (progress>75 || !BASS_StreamGetFilePosition(global_chan,BASS_FILEPOS_CONNECTED)) 
				{ 
					// over 75% full (or end of download)
					//KillTimer(global_hwnd,0); // finished prebuffering, stop monitoring
					KillTimer(global_hwnd,global_hwnd_timerid_prebuffermonitoring); // finished prebuffering, stop monitoring
					{ 
						// get the broadcast name and URL
						const char *icy=BASS_ChannelGetTags(global_chan,BASS_TAG_ICY);
						if (!icy) icy=BASS_ChannelGetTags(global_chan,BASS_TAG_HTTP); // no ICY tags, try HTTP
						if (icy) 
						{
							for (;*icy;icy+=strlen(icy)+1) 
							{
								if (!strnicmp(icy,"icy-name:",9))
								{
									/*
									MESS(31,WM_SETTEXT,0,icy+9);
									*/
									StatusAddText(icy);
									StatusAddText("\n");
								}
								if (!strnicmp(icy,"icy-url:",8))
								{
									/*
									MESS(32,WM_SETTEXT,0,icy+8);
									*/
									StatusAddText(icy);
									StatusAddText("\n");
								}
							}
						} 
						else
						{
							/*
							MESS(31,WM_SETTEXT,0,"");
							*/
							StatusAddText("\n");
						}
					}

	// get the stream title and set sync for subsequent titles
	DoMeta();
	BASS_ChannelSetSync(global_chan,BASS_SYNC_META,0,&MetaSync,0); // Shoutcast
	BASS_ChannelSetSync(global_chan,BASS_SYNC_OGG_CHANGE,0,&MetaSync,0); // Icecast/OGG
	// set sync for end of stream
	BASS_ChannelSetSync(global_chan,BASS_SYNC_END,0,&EndSync,0);

					// play it!
					BASS_ChannelPlay(global_chan,FALSE);
				} 
				else 
				{
					char text[20];
					sprintf(text,"buffering... %d%%",progress);
					//MESS(31,WM_SETTEXT,0,text);
					StatusAddText(text);
					StatusAddText("\n");
				}
			}
			else if(wParam==global_hwnd_timerid_skipupdatespectrum)
			{
				global_skip_updatespectrum=0;
				KillTimer(global_hwnd,global_hwnd_timerid_skipupdatespectrum);
			}
		}
		break;

	case WM_COMMAND:
		wmId    = LOWORD(wParam);
		wmEvent = HIWORD(wParam);
		// Parse the menu selections:
		switch (wmId)
		{
		case IDM_ABOUT:
			DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
			break;
		case IDM_EXIT:
			DestroyWindow(hWnd);
			break;
		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
		}
		break;
	case WM_PAINT:
		hdc = BeginPaint(hWnd, &ps);
		// TODO: Add any drawing code here...
		SetStretchBltMode(hdc, COLORONCOLOR);
		//spi, begin
		/*
		StretchDIBits(hdc, 0, 0, global_xwidth, global_yheight,
						0, 0, FreeImage_GetWidth(global_dib), FreeImage_GetHeight(global_dib),
						FreeImage_GetBits(global_dib), FreeImage_GetInfo(global_dib), DIB_RGB_COLORS, SRCCOPY);
		*/
		StretchDIBits(hdc, 0, 0, global_imagewidth, global_imageheight,
						0, 0, FreeImage_GetWidth(global_dib), FreeImage_GetHeight(global_dib),
						FreeImage_GetBits(global_dib), FreeImage_GetInfo(global_dib), DIB_RGB_COLORS, SRCCOPY);
		//spi, end
		hOldBrush = SelectObject(hdc, (HBRUSH)GetStockObject(GRAY_BRUSH));
		hOldPen = SelectObject(hdc, (HPEN)GetStockObject(WHITE_PEN));
		//iOldMixMode = SetROP2(hdc, R2_MASKPEN);
		iOldMixMode = SetROP2(hdc, R2_MERGEPEN);
		//Rectangle(hdc, 100, 100, 200, 200);

		crOldBkColor = SetBkColor(hdc, RGB(0xFF, 0x00, 0x00));
		crOldTextColor = SetTextColor(hdc, RGB(0xFF, 0xFF, 0xFF));
		iOldBkMode = SetBkMode(hdc, TRANSPARENT);
		//hFont=CreateFontW(70,0,0,0,FW_BOLD,0,0,0,0,0,0,2,0,L"SYSTEM_FIXED_FONT");
		//hOldFont=(HFONT)SelectObject(hdc,global_hFont);
		hOldFont=(HFONT)SelectObject(hdc,global_hFont);
		GetTextMetrics(hdc, &myTEXTMETRIC);
		global_fontwidth = myTEXTMETRIC.tmAveCharWidth;
		//TextOutW(hdc, 100, 100, L"test string", 11);

		SelectObject(hdc, hOldBrush);
		SelectObject(hdc, hOldPen);
		SetROP2(hdc, iOldMixMode);
		SetBkColor(hdc, crOldBkColor);
		SetTextColor(hdc, crOldTextColor);
		SetBkMode(hdc, iOldBkMode);
		SelectObject(hdc,hOldFont);
		//DeleteObject(hFont);

		//BitBlt(hdc,0,0,SPECWIDTH,SPECHEIGHT,specdc,0,0,SRCCOPY);

		EndPaint(hWnd, &ps);
		break;

	case WM_LBUTTONUP:
		specmode=(specmode+1)%4; // swap spectrum mode
		memset(specbuf,0,SPECWIDTH*SPECHEIGHT);	// clear display
		break;

	case WM_DESTROY:
		{
			//spi, begin, code from spispectrumplay_xaos
			KillTimer(global_hwnd, global_hwnd_timerid_skipupdatespectrum);
			KillTimer(global_hwnd, global_hwnd_timerid_prebuffermonitoring);
			if (global_timer_updatespectrum) timeKillEvent(global_timer_updatespectrum);
			if (specdc) DeleteDC(specdc);
			if (specbmp) DeleteObject(specbmp);
			//spi, end, code from spispectrumplay_xaos
			WavSetLib_Terminate();
			if (global_timer) timeKillEvent(global_timer);
			FreeImage_Unload(global_dib);
			DeleteObject(global_hFont);
			BASS_Free();

			int nShowCmd = false;
			//ShellExecuteA(NULL, "open", "end.bat", "", NULL, nShowCmd);
			ShellExecuteA(NULL, "open", global_end.c_str(), "", NULL, 0);
			PostQuitMessage(0);
		}
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	switch (message)
	{
	case WM_INITDIALOG:
		return (INT_PTR)TRUE;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
		{
			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
		break;
	}
	return (INT_PTR)FALSE;
}
