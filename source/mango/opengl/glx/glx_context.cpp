/*
    MANGO Multimedia Development Platform
    Copyright (C) 2012-2016 Twilight Finland 3D Oy Ltd. All rights reserved.
*/
#include <mango/core/exception.hpp>
#include <mango/core/string.hpp>
#include <mango/opengl/opengl.hpp>
#include "../../gui/xlib/xlib_handle.hpp"

#define ID ""

namespace
{
    using namespace mango;

    template <typename ContainerType>
    void parseExtensionString(ContainerType& container, const char* ext)
    {
        for (const char* s = ext; *s; ++s)
        {
            if (*s == ' ')
            {
                const std::ptrdiff_t length = s - ext;
                if (length > 0)
                {
                    container.insert(std::string(ext, length));
                }
                ext = s + 1;
            }
        }
    }

    int contextErrorHandler(Display* display, XErrorEvent* event)
    {
        (void) display;
        (void) event;
        return 0;
    }

} // namespace

namespace mango {
namespace opengl {

    // -----------------------------------------------------------------------
    // ContextHandle
    // -----------------------------------------------------------------------

    struct ContextHandle
    {
		GLXContext context { 0 };
        bool fullscreen { false };
    };

    static void deleteContext(WindowHandle* window_handle, ContextHandle* context_handle)
    {
        if (window_handle->display)
        {
            glXMakeCurrent(window_handle->display, 0, 0);

            if (context_handle->context) {
                glXDestroyContext(window_handle->display, context_handle->context);
                context_handle->context = 0;
            }
        }

        delete context_handle;
    }

    // -----------------------------------------------------------------------
    // Context
    // -----------------------------------------------------------------------

    Context::Context(int width, int height, const ContextAttribute* contextAttribute, Context* shared)
	: Window(width, height)
    {
        m_context = new ContextHandle();

        // TODO
        if (shared) {
            MANGO_EXCEPTION(ID"Shared context is not implemented yet.");
        }

        // Configure attributes
        ContextAttribute attrib;
        if (contextAttribute)
        {
            // Override defaults
            attrib = *contextAttribute;
        }

        std::vector<int> visualAttribs;

        visualAttribs.push_back(GLX_X_RENDERABLE);
        visualAttribs.push_back(True);

        visualAttribs.push_back(GLX_DRAWABLE_TYPE);
        visualAttribs.push_back(GLX_WINDOW_BIT);

        visualAttribs.push_back(GLX_RENDER_TYPE);
        visualAttribs.push_back(GLX_RGBA_BIT);

        visualAttribs.push_back(GLX_X_VISUAL_TYPE);
        visualAttribs.push_back(GLX_TRUE_COLOR);

        visualAttribs.push_back(GLX_DOUBLEBUFFER);
        visualAttribs.push_back(True);

        visualAttribs.push_back(GLX_RED_SIZE);
        visualAttribs.push_back(attrib.red);

        visualAttribs.push_back(GLX_GREEN_SIZE);
        visualAttribs.push_back(attrib.green);

        visualAttribs.push_back(GLX_BLUE_SIZE);
        visualAttribs.push_back(attrib.blue);

        visualAttribs.push_back(GLX_ALPHA_SIZE);
        visualAttribs.push_back(attrib.alpha);

        visualAttribs.push_back(GLX_DEPTH_SIZE);
        visualAttribs.push_back(attrib.depth);

        visualAttribs.push_back(GLX_STENCIL_SIZE);
        visualAttribs.push_back(attrib.stencil);

        if (attrib.samples > 1)
        {
            visualAttribs.push_back(GLX_SAMPLE_BUFFERS);
            visualAttribs.push_back(1);

            visualAttribs.push_back(GLX_SAMPLES);
            visualAttribs.push_back(attrib.samples);
        }

        visualAttribs.push_back(None);


        int glx_major, glx_minor;

        if (!glXQueryVersion(m_handle->display, &glx_major, &glx_minor))
        {
            deleteContext(m_handle, m_context);
            m_context = NULL;
            MANGO_EXCEPTION(ID"glXQueryVersion() failed.");
        }

        printf("GLX version: %d.%d\n", glx_major, glx_minor);

        if ((glx_major == 1 && glx_minor < 3) || glx_major < 1)
        {
            deleteContext(m_handle, m_context);
            m_context = NULL;
            MANGO_EXCEPTION(ID"Invalid GLX version.");
        }

        int fbcount;
        GLXFBConfig* fbc = glXChooseFBConfig(m_handle->display, DefaultScreen(m_handle->display), visualAttribs.data(), &fbcount);
        if (!fbc)
        {
            deleteContext(m_handle, m_context);
            m_context = NULL;
            MANGO_EXCEPTION(ID"glXChooseFBConfig() failed.");
        }

        printf("Found %d matching FB configs.\n", fbcount);

        // Pick the FB config/visual with the most samples per pixel
        int best_fbc = -1, worst_fbc = -1, best_num_samp = -1, worst_num_samp = 999;

        for (int i = 0; i < fbcount; ++i)
        {
            XVisualInfo* vi = glXGetVisualFromFBConfig(m_handle->display, fbc[i]);
            if (vi)
            {
                int samp_buf, samples;
                glXGetFBConfigAttrib(m_handle->display, fbc[i], GLX_SAMPLE_BUFFERS, &samp_buf);
                glXGetFBConfigAttrib(m_handle->display, fbc[i], GLX_SAMPLES       , &samples );

                printf("  Matching fbconfig %d, visual ID 0x%2x: SAMPLE_BUFFERS = %d, SAMPLES = %d\n",
                    i, (unsigned int)vi -> visualid, samp_buf, samples );

                if ((best_fbc < 0) || (samp_buf && samples > best_num_samp))
                    best_fbc = i, best_num_samp = samples;

                if ((worst_fbc < 0) || !samp_buf || (samples < worst_num_samp))
                    worst_fbc = i, worst_num_samp = samples;
            }

            XFree(vi);
        }

        GLXFBConfig bestFbc = fbc[best_fbc];
        XFree(fbc);

        XVisualInfo* vi = glXGetVisualFromFBConfig(m_handle->display, bestFbc);

        // create window
        if (!m_handle->createWindow(vi, width, height, "OpenGL"))
        {
            deleteContext(m_handle, m_context);
            m_context = NULL;
            MANGO_EXCEPTION(ID"createWindow() failed.");
        }

        XFree(vi);

        // Get the default screen's GLX extension list
        const char* glxExts = glXQueryExtensionsString(m_handle->display, DefaultScreen(m_handle->display));

        // Create GLX extension set
        std::set<std::string> glxExtensions;
        if (glxExts)
        {
            parseExtensionString(glxExtensions, glxExts);
        }

        // NOTE: It is not necessary to create or make current to a context before calling glXGetProcAddressARB
        PFNGLXCREATECONTEXTATTRIBSARBPROC glXCreateContextAttribsARB =
            (PFNGLXCREATECONTEXTATTRIBSARBPROC)glXGetProcAddressARB((const GLubyte *)"glXCreateContextAttribsARB");

        // Detect extension.
        bool isGLX_ARB_create_context = glxExtensions.find("GLX_ARB_create_context") != glxExtensions.end();

        m_context->context = 0;

        // Install an X error handler so the application won't exit if GL 3.0
        // context allocation fails.
        //
        // Note this error handler is global.  All display connections in all threads
        // of a process use the same error handler, so be sure to guard against other
        // threads issuing X commands while this code is running.
        int (*oldHandler)(Display*, XErrorEvent*) = XSetErrorHandler(&contextErrorHandler);

        // Check for the GLX_ARB_create_context extension string and the function.
        if (isGLX_ARB_create_context && glXCreateContextAttribsARB)
        {
            int context_attribs[] =
            {
#ifdef MANGO_CORE_PROFILE
                //GLX_CONTEXT_MAJOR_VERSION_ARB, 3,
                //GLX_CONTEXT_MINOR_VERSION_ARB, 0,
                GLX_CONTEXT_PROFILE_MASK_ARB,  GLX_CONTEXT_CORE_PROFILE_BIT_ARB,
#endif
                //GLX_CONTEXT_PROFILE_MASK_ARB,  GLX_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB,
                GLX_CONTEXT_FLAGS_ARB,         GLX_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB,
                None
            };

            m_context->context = glXCreateContextAttribsARB(m_handle->display, bestFbc, 0, True, context_attribs);

            // Sync to ensure any errors generated are processed.
            XSync(m_handle->display, False);

            if (m_context->context)
            {
                printf("Created GL 3.0 context\n");
            }
            else
            {
                printf("Failed to create GL 3.0 context ... using old-style GLX context\n");
                m_context->context = glXCreateContextAttribsARB(m_handle->display, bestFbc, 0, True, NULL);
            }
        }
        else
        {
            printf("glXCreateContextAttribsARB() not found ... using old-style GLX context\n");
            m_context->context = glXCreateNewContext(m_handle->display, bestFbc, GLX_RGBA_TYPE, 0, True);
        }

        // Sync to ensure any errors generated are processed.
        XSync(m_handle->display, False);

        // Restore the original error handler
        XSetErrorHandler(oldHandler);

        if (!m_context->context)
        {
            deleteContext(m_handle, m_context);
            m_context = nullptr;
            MANGO_EXCEPTION(ID"OpenGL Context creation failed.");
        }

        // Verifying that context is a direct context
        if (!glXIsDirect(m_handle->display, m_context->context))
        {
            printf("Indirect GLX rendering context obtained.\n");
        }
        else
        {
            printf("Direct GLX rendering context obtained.\n");
        }

        // TODO: configuration selection API
        // TODO: context version selection: 4.3, 3.2, etc.
        // TODO: initialize GLX extensions using GLEXT headers
        glXMakeCurrent(m_handle->display, m_handle->window, m_context->context);

#if 0
            PFNGLGETSTRINGIPROC glGetStringi = (PFNGLGETSTRINGIPROC)glXGetProcAddress((const GLubyte*)"glGetStringi");

            if (glGetStringi == NULL)
            {
                // Fall-back to the pre-3.0 method for querying extensions.
                std::string extensionString = (const char*)glGetString(GL_EXTENSIONS);
                m_extensions = tokenizeString(extensionString, " ");
            }
            else
            {
                // Use the post-3.0 method for querying extensions
                GLint numExtensions = 0;
                glGetIntegerv(GL_NUM_EXTENSIONS, &numExtensions);

                for (int i = 0; i < numExtensions; ++i)
                {
                    std::string ext = (const char*)glGetStringi(GL_EXTENSIONS, i);
                    m_extensions.push_back(ext);
                }
            }
#endif

        // parse extension string
        const GLubyte* extensions = glGetString(GL_EXTENSIONS);
        if (extensions)
        {
            parseExtensionString(m_extensions, reinterpret_cast<const char*>(extensions));
        }

        // initialize extension mask
        initExtensionMask();
    }

    Context::~Context()
    {
        deleteContext(m_handle, m_context);
    }

    void Context::makeCurrent()
    {
        glXMakeCurrent(m_handle->display, m_handle->window, m_context->context);
    }

    void Context::swapBuffers()
    {
        glXSwapBuffers(m_handle->display, m_handle->window);
    }

    void Context::swapInterval(int interval)
    {
        // TODO: detect extension, then use it
    }

    void Context::toggleFullscreen()
    {
        // Disable rendering while switching fullscreen mode
        m_handle->busy = true;
        glXMakeCurrent(m_handle->display, 0, 0);

        XEvent xevent;
        std::memset(&xevent, 0, sizeof(xevent));

        xevent.type = ClientMessage;
        xevent.xclient.window = m_handle->window;
        xevent.xclient.message_type = m_handle->atom_state;
        xevent.xclient.format = 32;
        xevent.xclient.data.l[0] = 2; // NET_WM_STATE_TOGGLE
        xevent.xclient.data.l[1] = m_handle->atom_fullscreen;
        xevent.xclient.data.l[2] = 0; // no second property to toggle
        xevent.xclient.data.l[3] = 1; // source indication: application
        xevent.xclient.data.l[4] = 0; // unused

        XMapWindow(m_handle->display, m_handle->window);

        // send the event to the root window
        if (!XSendEvent(m_handle->display, DefaultRootWindow(m_handle->display), False,
            SubstructureRedirectMask | SubstructureNotifyMask, &xevent))
        {
            // TODO: failed
        }

        XFlush(m_handle->display);

        // Enable rendering now that all the tricks are done
        m_handle->busy = false;
        glXMakeCurrent(m_handle->display, m_handle->window, m_context->context);

        m_context->fullscreen = !m_context->fullscreen;
    }

    bool Context::isFullscreen() const
	{
		return m_context->fullscreen;
	}

} // namespace opengl
} // namespace mango
