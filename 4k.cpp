/*
 * c++ 4k
 *
 * this is the source for our 4k, which ranked 2nd at dialogos 2001
 * 
 * you may use, whatever you want. but give us some credits.
 *
 * to achive the original filesize of 4072byte you have to use PE2COM 
 * and APACK.
 *
 * delta9 & franky
 */


#include <windows.h>
#include <gl/gl.h>
#include <gl/glu.h>
#include <math.h>

#pragma pack(1)

const int width=640;
const int height=480;

const int SR = 44100;
const int bits = 16;
const bool stereo = true;

const short align = ( stereo == true ) ? bits >> 2 : bits >> 3;

const int msec = 250;
const int BufSize = (align * SR * msec / 1000) & 0xfff0;


void *BufPtr1,
     *BufPtr2;

DWORD BufSamps1,
      BufSamps2;

DWORD MixPos,
      PlayPos,
      BufBytes1,
      BufBytes2;

WAVEFORMATEX pcmwf;
DSBUFFERDESC dsbdesc;
LPDIRECTSOUND lpDirectSound = NULL;
LPDIRECTSOUNDBUFFER lplpDsb = NULL;

struct T_LRC {
        float  iL,                     // Spulen-Strom
               iC,                     // Kondensator-Strom
               uC;                     // Kondensator-Spannung

        float  reso, cut;              // Widerstrand, L = C

};


void inline lrc(T_LRC &flt, float in ) {        // LRC bandpass

       flt.iL += flt.cut * flt.uC;
       flt.iC = (in - flt.uC) * flt.reso - flt.iL;
       flt.uC += flt.cut * flt.iC;
}


const int BPM = 142;
const int SPEED = 6;
const int HERTZ = BPM * 2 / 5;
const int ROWTICK = SR*SPEED/HERTZ;

int tick = 0;

// *** Bd ***

float s1,c1,amp,f;

#define BASS_LEN 16384

int tickBD = 0;

#define sqr(a) ( (a) * (a) )

// saw
float tsaw, sawfrq, sawenv;
T_LRC flt;

int id = 0;
char tb[] = { 1,1,1,2,
              0,1,1,0,
              1,1,1,2,
			        0,1,1,0};

// --- robo ---

#define SRATE   44100
#define Ta      ( 1.0f / SRATE )

#define M_PI    3.141592654f
#define sqr(a)  ( (a)*(a) )
#define exp2(x)	( pow( 2.0, x * 1.442695041 ) )

typedef struct {                // digital designed 12dB IIR resonator
        float  a, b, c;         // filter vars, coeff's
        float  y, z1, z2;       // (2 pole, 12 dB)

} T_BP;

T_BP		bp[ 2 ];

void bp_init( T_BP &bp, float f, float bw ) 
{
  float r = (float) exp2( (float)-M_PI*bw*Ta );
  bp.c = - r*r;
  bp.b = r * 2.0f * (float)cos( 2.0f*M_PI*f*Ta );
  bp.a = 1.0f - bp.b - bp.c;
}

void bp_iir( T_BP &bp, float in ) {	// 12 dB bi-quad
  bp.y = bp.a*in + bp.b*bp.z1 + bp.c*bp.z2;
  bp.z2 = bp.z1;
  bp.z1 = bp.y;
}

float	fm1 = 0.0f, fm2 = 0.0f,
      bw1 = 0.0f, bw2 = 0.0f,
      am1 = 0.0f, am2 = 0.0f,
      as = 0.0f, saw = 0.0f, mul;

float  asp, a1, a2, b1, b2, f1, f2;

int  frame = 0, robo_id = 0;

unsigned short input[] = {
10, 10,  10, 10, 0, ROWTICK*4*3 + ROWTICK,	//   silence
10, 10,  10, 10, 0, ROWTICK*4*3 + ROWTICK,	//   silence
190, 2680,         100, 100,     	255,     1000,   	// d
300, 2200,         100, 150,         1 ,      10000,	   //    i
700, 1100,         100, 100,         1,       10000,     // a
460, 1480,         100, 100,         1,       6000,   // l
610, 880,          100, 100,         1,       10000,    //o
550, 2000, 	      0, 0,      	1 ,      1500,		 //silence
190, 1480,         100, 100,        255,     1000,    // k
610, 880,          100, 100,         1,       10000,    //o
2620, 13000,        3, 10,        	255,     6000,    // s
10, 10,  10, 10, 0,ROWTICK*4,	//   silence
490, 1180,         100, 100,         1,       4000 ,  //   r
300, 950,          100, 100,         1,       7000,     // u
460, 1480,         100, 100,         1,       6000,   // l
2620, 13000,        3, 10,        	255,     6000,    // s
};

void render( void *ptr, int len ) {

	short *b = (short*)ptr;

	for ( int i=0; i<len*2; i+=2 ) {

		if ( --tick < 0 ) {

			tick += ROWTICK;

			if ( --tickBD < 0 ) {

				amp = 1.0f;
				f = .03f;
				c1 = 1.0f;
				s1 = 0.0f;
				tickBD += 4;

			}

			// tb

			sawfrq = 0.002f * tb[id];
			sawenv = 1.0f;
			++id &= 15;

		}

		// --- bd ---
		amp *= 0.9995f;
		f *= 0.9995f;

		s1 += c1 * f;
		c1 -= s1 * f;


		// --- tb ---

		sawenv *= 0.9999f;
		tsaw += sawfrq - (int)tsaw;
		flt.cut = 0.01f + sawenv*0.06f;
		flt.reso = 0.05f;
		lrc( flt, tsaw );

		// --- robo ---

		if ( --frame < 0 ) {
	
			f1 = input[ robo_id ];
			robo_id++;
			f2 = input[ robo_id ];
			robo_id++;
			a1 = input[ robo_id ];
			robo_id++;
			a2 = input[ robo_id ];
			robo_id++;
			asp = input[ robo_id ] / 256.0f;
			robo_id++;
			frame = input[ robo_id ];
			robo_id++;

			if ( robo_id >= sizeof(input)/2 ) {

				robo_id = 0;

			}
			if ( asp ) mul = 1.0f; else mul=1.0f/512.0f;

		}
        fm1 += ( f1 - fm1 ) * mul;
        fm2 += ( f2 - fm2 ) * mul;
        am1 += ( a1 - am1 ) * mul;
        am2 += ( a2 - am2 ) * mul;
        as += ( asp - as ) * mul;

        bp_init( bp[0], fm1, 50 );
        bp_init( bp[1], fm2, 50 );

        saw += 90.0f/44100.0f - (int)saw;

        float src = (saw - .5f)*(1.0f-as) + as * ( (rand() & 255) / 256.0f - 0.5f);

        bp_iir( bp[0], src * am1 );
        bp_iir( bp[1], src * am2 );

        float robo_out = bp[0].y + bp[1].y;

		float out = (s1*amp + 2*flt.iL*sawenv + robo_out*0.001f) * 0.5f;
		b[i] = (int)(out * 32767.0);
		b[i+1] = (int)(-out * 32767.0);

	}

}

/*
	*** play & loop ringbuffer! ***
*/
void inline start() {

  memset( &bp, 0, sizeof bp );		// robo

  DirectSoundCreate( NULL, &lpDirectSound, NULL);

	HWND hWnd = GetForegroundWindow();
 
	lpDirectSound->SetCooperativeLevel( hWnd, DSSCL_NORMAL );

	// Set up wave format structure.
  memset( &pcmwf, 0, sizeof WAVEFORMATEX );
  pcmwf.wFormatTag = WAVE_FORMAT_PCM;
  pcmwf.nChannels = ( stereo == true ) ? 2 : 1;
  pcmwf.nSamplesPerSec = SR;
  pcmwf.nBlockAlign = align;
  pcmwf.nAvgBytesPerSec = SR * align;
  pcmwf.wBitsPerSample = bits;

	// --- start sound ---

	// Set up DSBUFFERDESC structure.
  memset(&dsbdesc, 0, sizeof DSBUFFERDESC); // Zero it out.
  dsbdesc.dwSize = sizeof DSBUFFERDESC;
  dsbdesc.dwFlags = DSBCAPS_STICKYFOCUS;
  dsbdesc.dwBufferBytes = BufSize;
  dsbdesc.lpwfxFormat = &pcmwf;

  // Try to create secondary buffer.
  lpDirectSound->CreateSoundBuffer(&dsbdesc, &(lplpDsb), NULL);
	
	// start ringbuffer output
	PlayPos = MixPos = 0;
	lplpDsb->Play(0, 0, DSBPLAY_LOOPING );

}

void inline stop() {
	lplpDsb->Stop();
}


void inline poll() {

	// get position
	DWORD writepos;

	do {
	
		lplpDsb->GetCurrentPosition( &PlayPos, &writepos );

	} while ( PlayPos >= BufSize );


	// lock
	int len = (int)PlayPos - (int)MixPos;
	if (len < 0) {

		len += BufSize;

	}
	
	HRESULT hr = lplpDsb->Lock(	MixPos, (unsigned long)len,
						&BufPtr1, &BufBytes1,
						&BufPtr2, &BufBytes2,
						0 );

	MixPos = PlayPos;

	// render

	render( BufPtr1, BufBytes1 >> (align >> 1) );
	render( BufPtr2, BufBytes2 >> (align >> 1) );


	// unlock
	lplpDsb->Unlock(	BufPtr1, BufBytes1,
						BufPtr2, BufBytes2 );

}



#ifndef M_PI
// #define M_PI 3.14159265f
float M_PI = 3.141f
#endif

#define FULLSCREEN

HWND hWnd;
HGLRC hRC;
HDC hDC;
HINSTANCE hInstance;

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message) 
	{
	case WM_CREATE:
	case WM_DESTROY:
	case WM_SIZE:
		return 0;
	case WM_CLOSE:
		PostQuitMessage(0);
		return 0;
	case WM_KEYDOWN:
		switch (wParam) 
		{
		case VK_ESCAPE:
		    PostQuitMessage(0);
		    return 0;
		}
		return 0;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
}

// The Effect
float		angle;
GLuint		BlurTexture;

// typedef struct { float  x,  y,  z; } Vector;
// Vector *vertices;
const int NUMSEG = 96;
const int SPIKES = 8;

float distortMagnitude;
float distortMagnitude2;
float radius;
float eta;
float x_thing, y_thing, z_thing;

GLvoid renderThing(float progress) {
	char i, j;
	distortMagnitude = sinf(progress*1.3f)*0.55f-0.35f;
	radius = 10.0f;
	eta = 0.0f;

	for (i=0; i<NUMSEG; i++) 
	{
		char k=0;
		float theta=0.0f;
		glBegin(GL_QUAD_STRIP);
		glColor3f(0.7f, 0.7f, 0.9f);
		for (j=0; j<NUMSEG; j++, k++) 
		{
			x_thing = (cosf(theta)*cosf(fee)*radius);
			y_thing = (sinf(theta)*cosf(fee)*radius);
			z_thing = sinf(fee)*radius;
			float distort = ((1-cosf(theta*SPIKES)) +
                       (1-cosf(fee*SPIKES)) +
                       (1-cosf(atan2f(x_thing, z_thing)*SPIKES)))
			                 /6.0f*distortMagnitude+1.0f;

      theta += M_PI*4.0f/NUMSEG;

      glVertex3f(x_thing*distort, y_thing*distort, z_thing*distort);
      glVertex3f(x_thing*distort, y_thing*distort+0.5f, z_thing*distort);
    }
    glEnd();
    eta +=M_PI*4.0f/NUMSEG;
	}
}

int WINAPI WinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {

	WNDCLASS wc;
	DWORD dwExStyle=WS_EX_APPWINDOW | WS_EX_WINDOWEDGE;
	DWORD dwStyle = WS_OVERLAPPEDWINDOW;

	RECT WindowRect;
	WindowRect.left=(long)0;
	WindowRect.right=(long)width;	
	WindowRect.top=(long)0;	
	WindowRect.bottom=(long)height;	

	wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
	wc.lpfnWndProc = WndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = hInstance;
	wc.hIcon = LoadIcon( NULL, IDI_APPLICATION );
	wc.hCursor = LoadCursor( NULL, IDC_ARROW );
	wc.hbrBackground = (HBRUSH)GetStockObject( BLACK_BRUSH );
	wc.lpszMenuName = NULL;
	wc.lpszClassName = "sGL";

	if (!RegisterClass( &wc ))	return;

#ifdef FULLSCREEN
		DEVMODE dmScreenSettings;
		memset(&dmScreenSettings,0,sizeof(dmScreenSettings));	
		dmScreenSettings.dmSize=sizeof(dmScreenSettings);
		dmScreenSettings.dmPelsWidth	= width;
		dmScreenSettings.dmPelsHeight	= height;
		dmScreenSettings.dmBitsPerPel	= 32;	
		dmScreenSettings.dmFields=DM_BITSPERPEL|DM_PELSWIDTH|DM_PELSHEIGHT;

		ChangeDisplaySettings(&dmScreenSettings,CDS_FULLSCREEN);

		dwExStyle=WS_EX_APPWINDOW;
		dwStyle=WS_POPUP;
		ShowCursor(FALSE);

		AdjustWindowRectEx(&WindowRect, dwStyle, FALSE, dwExStyle);
#endif

  hWnd=CreateWindowEx(	dwExStyle,
                        "sGL", "4k",
                        WS_CLIPSIBLINGS | WS_CLIPCHILDREN | dwStyle,
                        0, 0,	
                        WindowRect.right-WindowRect.left,
                        WindowRect.bottom-WindowRect.top,
                        NULL,arent Window
                        NULL, hInstance, NULL) ;
	hDC = GetDC(hWnd);

//	EnableOpenGL();

	hDC = GetDC(hWnd);

  // set the pixel format for the DC
  ZeroMemory( &pfd, sizeof( pfd ) );
  pfd.nSize = sizeof( pfd );
  pfd.nVersion = 1;
  pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
  pfd.iPixelType = PFD_TYPE_RGBA;
  pfd.cColorBits = 24;
  pfd.cDepthBits = 16;
  pfd.iLayerType = PFD_MAIN_PLANE;

	iFormat = ChoosePixelFormat(hDC, &pfd ); // exit if failure...
	SetPixelFormat(hDC, iFormat, &pfd );
	hRC = wglCreateContext(hDC);
	wglMakeCurrent(hDC, hRC);

	ShowWindow(hWnd, SW_SHOW);
	SetForegroundWindow(hWnd);
	SetFocus(hWnd);

	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glViewport(0, 0, width, height);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(45.0f, 1.5f, 0.1f, 551.0f);
	glMatrixMode(GL_MODELVIEW);
//	glLoadIdentity();


	unsigned int *data;
	data = (unsigned int*)new GLuint[((128 * 128)* 4 * sizeof(unsigned int))];
	//  ZeroMemory(data,((128 * 128)* 4 * sizeof(unsigned int)));	// Clear Storage Memory
  glGenTextures(1, &BlurTexture);                     // Create 1 Texture
  glBindTexture(GL_TEXTURE_2D, BlurTexture);          // Bind The Texture
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
	// delete[] data;

	float progress=0.0f;
	MSG msg;

	start();

	while(true) 
	{
		poll();

		PeekMessage( &msg, NULL, 0, 0, PM_REMOVE );

			if ( msg.message == WM_QUIT ) 
				return;
			else 
			{
				glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
				glLoadIdentity();

				glTranslatef(0.0f, 0.0f, -50.0f);
				glRotatef(progress*12.0f, 1.0f, 1.0f, 1.0f);

				glViewport(0,0,128,128);
				renderThing(progress);
				glBindTexture(GL_TEXTURE_2D,BlurTexture);
				glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, 0, 0, 128, 128, 0);
				glClearColor(0.0f, 0.0f, 0.0f, 0.5);
				glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
				glViewport(0 , 0,640 ,480);

				renderThing(progress);

				int times = 35;
				float inc = 0.015f;
				float spost = 0.0f;
				float alphainc = 0.9f / times;
				float alpha = 0.2f;
				glDisable(GL_TEXTURE_GEN_S);
				glDisable(GL_TEXTURE_GEN_T);

				glEnable(GL_TEXTURE_2D);
				glDisable(GL_DEPTH_TEST);
				glBlendFunc(GL_SRC_ALPHA,GL_ONE);
				glEnable(GL_BLEND);
				glBindTexture(GL_TEXTURE_2D,BlurTexture);

				glMatrixMode(GL_PROJECTION);			// Select Projection
				glPushMatrix();							// Push The Matrix
				glLoadIdentity();						// Reset The Matrix
				glOrtho( 0, 640 , 480 , 0, -1, 1 );		// Select Ortho Mode (640x480)
				glMatrixMode(GL_MODELVIEW);				// Select Modelview Matrix
				glPushMatrix();							// Push The Matrix
				glLoadIdentity();						// Reset The Matrix

				alphainc = alpha / times;

				glBegin(GL_QUADS);

				for (int num = 0;num < times;num++)
				{
					glColor4f(0.5f, 0.7f, 1.0f, alpha);
					glTexCoord2f(0+spost,1-spost);	
					glVertex2f(0,0);				

					glTexCoord2f(0+spost,0+spost);	
					glVertex2f(0,480);				

					glTexCoord2f(1-spost,0+spost);	
					glVertex2f(640,480);			

					glTexCoord2f(1-spost,1-spost);	
					glVertex2f(640,0);				

					spost += inc;					
					alpha = alpha - alphainc;		
				}
				glEnd();							

				glMatrixMode( GL_PROJECTION );
				glPopMatrix();
				glMatrixMode( GL_MODELVIEW );
				glPopMatrix();

				glEnable(GL_DEPTH_TEST);			
				glDisable(GL_TEXTURE_2D);			
				glDisable(GL_BLEND);				
				// glBindTexture(GL_TEXTURE_2D,0);

				SwapBuffers(hDC);
				progress+=0.2f;

				TranslateMessage( &msg );
				DispatchMessage( &msg );
			}		
	}

	stop();
	return;
}

// well, ok...
/*
void DisableOpenGL()
{
    wglMakeCurrent( NULL, NULL );
    wglDeleteContext(hRC);
    ReleaseDC(hWnd, hDC );
}

void KillWindow()
{
#ifdef FULLSCREEN
		ChangeDisplaySettings(NULL, 0);
		ShowCursor(true);
#endif
	if (hRC)
	{
		if (!wglMakeCurrent(NULL,NULL)) { 
			// MessageBox(NULL,".","SHUTDOWN ERROR",MB_OK | MB_ICONINFORMATION);
		} 
		if (!wglDeleteContext(hRC)) { 
			// MessageBox(NULL,".","SHUTDOWN ERROR",MB_OK | MB_ICONINFORMATION);
		}
		hRC = NULL;
	}
	if (hDC && !ReleaseDC(hWnd, hDC)) {
		// MessageBox(NULL,".","SHUTDOWN ERROR",MB_OK | MB_ICONINFORMATION);
		hDC=NULL;	
	}
	if (hWnd && !DestroyWindow(hWnd)) {
		// MessageBox(NULL,".","SHUTDOWN ERROR",MB_OK | MB_ICONINFORMATION);
		hWnd=NULL;							// Set hWnd To NULL
	}
	if (!UnregisterClass("sGL", hInstance))	{
		// MessageBox(NULL,".","SHUTDOWN ERROR",MB_OK | MB_ICONINFORMATION);
		hInstance=NULL;	
	}
	exit(0);
}
*/
