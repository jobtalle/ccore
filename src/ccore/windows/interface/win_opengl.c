#include "win_opengl.h"

ccReturn ccGLContextBind(void)
{
	int pixelFormatIndex;

	ccAssert(ccWindowExists());

	_CC_WINDOW_DATA->hdc = GetDC(_CC_WINDOW_DATA->winHandle);
	if(_CC_WINDOW_DATA->hdc == NULL) {
		ccErrorPush(CC_ERROR_GL_CONTEXT);
		return CC_FAIL;
	}

	PIXELFORMATDESCRIPTOR pfd = {
		sizeof(PIXELFORMATDESCRIPTOR),
		1,
		PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
		PFD_TYPE_RGBA,
		32,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		16,
		0, 0, 0, 0, 0, 0, 0
	};

	pixelFormatIndex = ChoosePixelFormat(_CC_WINDOW_DATA->hdc, &pfd);
	if(pixelFormatIndex == 0) {
		ccErrorPush(CC_ERROR_GL_CONTEXT);
		return CC_FAIL;
	}

	if(SetPixelFormat(_CC_WINDOW_DATA->hdc, pixelFormatIndex, &pfd) == FALSE) {
		ccErrorPush(CC_ERROR_GL_CONTEXT);
		return CC_FAIL;
	}

	_CC_WINDOW_DATA->renderContext = wglCreateContext(_CC_WINDOW_DATA->hdc);
	if(_CC_WINDOW_DATA->renderContext == NULL) {
		ccErrorPush(CC_ERROR_GL_CONTEXT);
		return CC_FAIL;
	}

	//Make window the current context
	if(wglMakeCurrent(_CC_WINDOW_DATA->hdc, _CC_WINDOW_DATA->renderContext) == FALSE) {
		ccErrorPush(CC_ERROR_GL_CONTEXT);
		return CC_FAIL;
	}

	return CC_SUCCESS;
}

ccReturn ccGLContextFree(void)
{
	ccAssert(_ccWindow != NULL);

	wglDeleteContext(_CC_WINDOW_DATA->renderContext);
	_CC_WINDOW_DATA->renderContext = NULL;

	return CC_SUCCESS;
}

ccReturn ccGLBuffersSwap(void)
{
	ccAssert(_ccWindow != NULL);
	if(SwapBuffers(_CC_WINDOW_DATA->hdc) == TRUE) {
		return CC_SUCCESS;
	}
	else{
		ccErrorPush(CC_ERROR_GL_BUFFERSWAP);
		return CC_FAIL;
	}
}

bool ccGLContextIsActive(void)
{
	return _CC_WINDOW_DATA->renderContext != NULL;
}
