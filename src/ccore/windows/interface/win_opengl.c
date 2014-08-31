#include "win_opengl.h"

ccReturn ccGLBindContext(int glVersionMajor, int glVersionMinor)
{
	int pixelFormatIndex;
	int glVerMajor, glVerMinor;

	ccAssert(ccWindowExists());

	WINDOW_DATA->hdc = GetDC(WINDOW_DATA->winHandle);

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

	pixelFormatIndex = ChoosePixelFormat(WINDOW_DATA->hdc, &pfd);
	SetPixelFormat(WINDOW_DATA->hdc, pixelFormatIndex, &pfd);

	WINDOW_DATA->renderContext = wglCreateContext(WINDOW_DATA->hdc);
	if(WINDOW_DATA->renderContext == NULL) {
		ccErrorPush(CC_ERROR_GLCONTEXT);
		return CC_FAIL;
	}

	//Make window the current context
	wglMakeCurrent(WINDOW_DATA->hdc, WINDOW_DATA->renderContext);

	//Version check
	glGetIntegerv(GL_MAJOR_VERSION, &glVerMajor);
	glGetIntegerv(GL_MINOR_VERSION, &glVerMinor);
	if(glVerMajor < glVersionMajor || (glVerMajor == glVersionMajor && glVerMinor < glVersionMinor)) {
		ccErrorPush(CC_ERROR_GLVERSION);
		return CC_FAIL;
	}

	//Fetch extentions after context creation
	if(glewInit() != GLEW_OK) {
		ccErrorPush(CC_ERROR_GLEWINIT);
		return CC_FAIL;
	}

	return CC_SUCCESS;
}

ccReturn ccGLFreeContext(void)
{
	ccAssert(_ccWindow != NULL);

	wglDeleteContext(WINDOW_DATA->renderContext);

	return CC_SUCCESS;
}

ccReturn ccGLSwapBuffers(void)
{
	ccAssert(_ccWindow != NULL);
	if(SwapBuffers(WINDOW_DATA->hdc) == TRUE) {
		return CC_SUCCESS;
	}
	else{
		ccErrorPush(CC_ERROR_CANTSWAP);
		return CC_FAIL;
	}
}