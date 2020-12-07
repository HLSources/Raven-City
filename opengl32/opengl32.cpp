//========= Copyright © 2004-2008, Paranoia Team and Raven City Team, All rights reserved. =============//
//																										//
// Purpose: Useful ogl functions for half-life including hooked extensions								//
//																										//
// $NoKeywords: $																						//
//======================================================================================================//

#include "opengl32.h"

void sys_glMultiTexCoord2fARB(GLenum target, GLfloat s, GLfloat t)
{
	orig_glMultiTexCoord2fARB(target,s,t);
}

void sys_glActiveTextureARB(GLenum target)
{
	orig_glActiveTextureARB(target);
}

void sys_BindTextureEXT(GLenum target, GLuint texture)
{
	orig_BindTextureEXT(target,texture);
}

void sys_glAlphaFunc (GLenum func,  GLclampf ref)
{
	(*orig_glAlphaFunc) (func, ref);
}

void sys_glBegin (GLenum mode)
{
	(*orig_glBegin) (mode);
}

void sys_glBitmap (GLsizei width,  GLsizei height,  GLfloat xorig,  GLfloat yorig,  GLfloat xmove,  GLfloat ymove,  const GLubyte *bitmap)
{
	(*orig_glBitmap) (width, height, xorig, yorig, xmove, ymove, bitmap);
}

void sys_glBlendFunc (GLenum sfactor,  GLenum dfactor)
{
	(*orig_glBlendFunc) (sfactor, dfactor);
}

void sys_glClear (GLbitfield mask)
{
	// buz: clear also stencil buffer
	int hasStencil;
	glGetIntegerv(GL_STENCIL_BITS, &hasStencil);
	if (hasStencil)
	{
		mask |= GL_STENCIL_BUFFER_BIT;
		glClearStencil(0);
	}

	(*orig_glClear)(mask); 
}

void sys_glClearAccum (GLfloat red,  GLfloat green,  GLfloat blue,  GLfloat alpha)
{
	(*orig_glClearAccum) (red, green, blue, alpha);
}

void sys_glClearColor (GLclampf red,  GLclampf green,  GLclampf blue,  GLclampf alpha)
{
	(*orig_glClearColor) (red, green, blue, alpha);
}

void sys_glColor3f (GLfloat red,  GLfloat green,  GLfloat blue)
{
	(*orig_glColor3f) (red, green, blue);
}

void sys_glColor3ub (GLubyte red,  GLubyte green,  GLubyte blue)
{
	(*orig_glColor3ub) (red, green, blue);
}

void sys_glColor3ubv (const GLubyte *v)
{
	(*orig_glColor3ubv) (v);
}

void sys_glColor4f (GLfloat red,  GLfloat green,  GLfloat blue,  GLfloat alpha)
{
	(*orig_glColor4f) (red, green, blue, alpha);
}

void sys_glColor4ub (GLubyte red,  GLubyte green,  GLubyte blue,  GLubyte alpha)
{
	(*orig_glColor4ub) (red, green, blue, alpha);
}

void sys_glCullFace (GLenum mode)
{
	(*orig_glCullFace) (mode);
}

void sys_glDeleteTextures (GLsizei n,  const GLuint *textures)
{
	(*orig_glDeleteTextures) (n, textures);
}

void sys_glDepthFunc (GLenum func)
{
	(*orig_glDepthFunc) (func);
}

void sys_glDepthMask (GLboolean flag)
{
	(*orig_glDepthMask) (flag);
}

void sys_glDepthRange (GLclampd zNear,  GLclampd zFar)
{
	// buz
	if (zNear == 0 && (zFar == 0.5 || zFar == 1))
		zFar = 0.8;

	(*orig_glDepthRange) (zNear, zFar);
}

void sys_glDisable (GLenum cap)
{
	(*orig_glDisable) (cap);
}

void sys_glEnable (GLenum cap)
{
	(*orig_glEnable) (cap);
}

void sys_glEnd (void)
{
	(*orig_glEnd) ();
}

void sys_glFrustum (GLdouble left,  GLdouble right,  GLdouble bottom,  GLdouble top,  GLdouble zNear,  GLdouble zFar)
{
	(*orig_glFrustum) (left, right, bottom, top, zNear, zFar);
}

void sys_glOrtho (GLdouble left,  GLdouble right,  GLdouble bottom,  GLdouble top,  GLdouble zNear,  GLdouble zFar)
{
	(*orig_glOrtho) (left, right, bottom, top, zNear, zFar);
}

void sys_glPopMatrix (void)
{
	(*orig_glPopMatrix) ();
}

void sys_glPopName (void)
{
	(*orig_glPopName) ();
}

void sys_glPrioritizeTextures (GLsizei n,  const GLuint *textures,  const GLclampf *priorities)
{
	(*orig_glPrioritizeTextures) (n, textures, priorities);
}

void sys_glPushAttrib (GLbitfield mask)
{
	(*orig_glPushAttrib) (mask);
}

void sys_glPushClientAttrib (GLbitfield mask)
{
	(*orig_glPushClientAttrib) (mask);
}

void sys_glPushMatrix (void)
{
	(*orig_glPushMatrix) ();
}

void sys_glRotatef (GLfloat angle,  GLfloat x,  GLfloat y,  GLfloat z)
{
	(*orig_glRotatef) (angle, x, y, z);
}

void sys_glShadeModel (GLenum mode)
{
	(*orig_glShadeModel) (mode);
}

void sys_glTexCoord2f (GLfloat s,  GLfloat t)
{
	(*orig_glTexCoord2f) (s, t);
}

void sys_glTexEnvf (GLenum target,  GLenum pname,  GLfloat param)
{
	(*orig_glTexEnvf) (target, pname, param);
}

void sys_glTexImage2D (GLenum target,  GLint level,  GLint internalformat,  GLsizei width,  GLsizei height,  GLint border,  GLenum format,  GLenum type,  const GLvoid *pixels)
{
	(*orig_glTexImage2D) (target, level, internalformat, width, height, border, format, type, pixels);
}

void sys_glTexParameterf (GLenum target,  GLenum pname,  GLfloat param)
{
	(*orig_glTexParameterf) (target, pname, param);
}

void sys_glTranslated (GLdouble x,  GLdouble y,  GLdouble z)
{
	(*orig_glTranslated) (x, y, z);
}

void sys_glTranslatef (GLfloat x,  GLfloat y,  GLfloat z)
{
	(*orig_glTranslatef) (x, y, z);
}

void sys_glVertex2f (GLfloat x,  GLfloat y)
{
	(*orig_glVertex2f) (x, y);
}

void sys_glVertex3f (GLfloat x,  GLfloat y,  GLfloat z)
{
	(*orig_glVertex3f) (x, y, z);
}

void sys_glVertex3fv (const GLfloat *v)
{
	(*orig_glVertex3fv) (v);
}

void sys_glViewport (GLint x,  GLint y,  GLsizei width,  GLsizei height)
{
	(*orig_glViewport) (x, y, width, height);
}

PROC sys_wglGetProcAddress(LPCSTR ProcName)
{
	if (!strcmp(ProcName,"glMultiTexCoord2fARB"))
	{
		orig_glMultiTexCoord2fARB=(func_glMultiTexCoord2fARB_t)orig_wglGetProcAddress(ProcName);
		return (FARPROC)sys_glMultiTexCoord2fARB;
	}
	else if (!strcmp(ProcName,"glBindTextureEXT"))
	{
		orig_BindTextureEXT=(func_BindTextureEXT_t)orig_wglGetProcAddress(ProcName);
		return (FARPROC)sys_BindTextureEXT;
	}
	else if(!strcmp(ProcName,"glActiveTextureARB"))
	{
		orig_glActiveTextureARB=(func_glActiveTextureARB_t)orig_wglGetProcAddress(ProcName);
		return (FARPROC)sys_glActiveTextureARB;
	}
	return orig_wglGetProcAddress(ProcName);
}

void sys_wglSwapBuffers(HDC hDC)
{
	(*orig_wglSwapBuffers) (hDC);
}



void __cdecl add_log (const char * fmt, ...);


#pragma warning(disable:4100)
BOOL __stdcall DllMain (HINSTANCE hinstDll, DWORD fdwReason, LPVOID lpvReserved)
{
	switch(fdwReason)
	{
		case DLL_PROCESS_ATTACH:
			DisableThreadLibraryCalls (hOriginalDll);
			return Init();

		case DLL_PROCESS_DETACH:
			if ( hOriginalDll != NULL )
			{
				FreeLibrary(hOriginalDll);
				hOriginalDll = NULL;
			}
			break;
	}
	return TRUE;
}
#pragma warning(default:4100)


void __cdecl add_log (const char * fmt, ...)
{
	va_list va_alist;
	char logbuf[256] = "";
    FILE * fp;
   
	va_start (va_alist, fmt);
	_vsnprintf (logbuf+strlen(logbuf), sizeof(logbuf) - strlen(logbuf), fmt, va_alist);
	va_end (va_alist);

	if ( (fp = fopen ("opengl32.log", "ab")) != NULL )
	{
		fprintf ( fp, "%s\n", logbuf );
		fclose (fp);
	}
}