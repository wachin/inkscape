// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef INKSCAPE_UI_WIDGET_OPTGLAREA_H
#define INKSCAPE_UI_WIDGET_OPTGLAREA_H

#include <gtkmm.h>
#include <epoxy/gl.h>

namespace Cairo {
class Context;
}

namespace Inkscape {
namespace UI {
namespace Widget {

/**
 * A widget that can dynamically switch between a Gtk::DrawingArea and a Gtk::GLArea.
 * Based on the GTK source code for both widgets.
 */
class OptGLArea : public Gtk::DrawingArea
{
public:
    OptGLArea();

    /**
     * Set whether OpenGL is enabled. Initially it is disabled. Upon enabling it,
     * create_context will be called as soon as the widget is realized. If
     * context creation fails, OpenGL will be disabled again.
     */
    void set_opengl_enabled(bool);
    bool get_opengl_enabled() const { return opengl_enabled; }

    /**
     * Call before doing any OpenGL operations to make the context current.
     * Automatically done before calling opengl_render.
     */
    void make_current();

    /**
     * Call before rendering to the widget to bind the widget's framebuffer.
     */
    void bind_framebuffer() const;

protected:
    void on_realize() override;
    void on_unrealize() override;
    void on_size_allocate(Gtk::Allocation&) override;
    bool on_draw(const Cairo::RefPtr<Cairo::Context>&) final;

    /**
     * Reimplement to create the desired OpenGL context. Return nullptr on error.
     */
    virtual Glib::RefPtr<Gdk::GLContext> create_context() = 0;

    /**
     * Reimplement to render the widget. The Cairo context is only for when OpenGL is disabled.
     */
    virtual void paint_widget(const Cairo::RefPtr<Cairo::Context>&) {}

private:
    void init_opengl();
    void create_framebuffer();
    void delete_framebuffer();
    void resize_framebuffer() const;

    Glib::RefPtr<Gdk::GLContext> context;

    bool opengl_enabled;
    bool need_resize;

    GLuint framebuffer;
    GLuint renderbuffer;
    GLuint stencilbuffer;
};

} // namespace Widget
} // namespace UI
} // namespace Inkscape

#endif // INKSCAPE_UI_WIDGET_OPTGLAREA_H
