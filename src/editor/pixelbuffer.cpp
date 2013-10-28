/*
 * Copyright 2013, Tim Baker <treectrl@users.sf.net>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "pixelbuffer.h"

static void convertFromGLImage(QImage &img, int w, int h, bool alpha_format, bool include_alpha)
{
    if (QSysInfo::ByteOrder == QSysInfo::BigEndian) {
        // OpenGL gives RGBA; Qt wants ARGB
        uint *p = (uint*)img.bits();
        uint *end = p + w*h;
        if (alpha_format && include_alpha) {
            while (p < end) {
                uint a = *p << 24;
                *p = (*p >> 8) | a;
                p++;
            }
        } else {
            // This is an old legacy fix for PowerPC based Macs, which
            // we shouldn't remove
            while (p < end) {
                *p = 0xff000000 | (*p>>8);
                ++p;
            }
        }
    } else {
        // OpenGL gives ABGR (i.e. RGBA backwards); Qt wants ARGB
        for (int y = 0; y < h; y++) {
            uint *q = (uint*)img.scanLine(y);
            for (int x=0; x < w; ++x) {
                const uint pixel = *q;
                if (alpha_format && include_alpha) {
                    *q = ((pixel << 16) & 0xff0000) | ((pixel >> 16) & 0xff)
                         | (pixel & 0xff00ff00);
                } else {
                    *q = 0xff000000 | ((pixel << 16) & 0xff0000)
                         | ((pixel >> 16) & 0xff) | (pixel & 0x00ff00);
                }

                q++;
            }
        }

    }
    img = img.mirrored();
}

static QImage qt_gl_read_frame_buffer(const QSize &size, bool alpha_format, bool include_alpha)
{
    QImage img(size, (alpha_format && include_alpha) ? QImage::Format_ARGB32_Premultiplied
                                                     : QImage::Format_RGB32);
    int w = size.width();
    int h = size.height();
    glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, img.bits());
    convertFromGLImage(img, w, h, alpha_format, include_alpha);
    return img;
}

PixelBuffer::PixelBuffer(const QSize &size, const QGLFormat &format, QGLWidget *shareWidget) :
    invalid(true),
    qctx(0),
    widget(0),
    fbo(0),
    blit_fbo(0)
{
    widget = new QGLWidget(format, 0, shareWidget);
    widget->resize(1, 1);
    qctx = const_cast<QGLContext *>(widget->context());

    if (widget->isValid()) {
        req_size = size;
        req_format = format;
        invalid = false;
    }
}

PixelBuffer::~PixelBuffer()
{
    QGLContext *current = const_cast<QGLContext *>(QGLContext::currentContext());
    if (current != qctx)
        makeCurrent();

    delete fbo;
    fbo = 0;
    delete blit_fbo;
    blit_fbo = 0;
    delete widget;
    widget = 0;

    if (current && current != qctx)
        current->makeCurrent();
}

bool PixelBuffer::makeCurrent()
{
    if (invalid)
        return false;
    qctx->makeCurrent();
    if (!fbo) {
        QOpenGLFramebufferObjectFormat format;
        if (req_format.stencil())
            format.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
        else if (req_format.depth())
            format.setAttachment(QOpenGLFramebufferObject::Depth);
        if (req_format.sampleBuffers())
            format.setSamples(req_format.samples());
        fbo = new QOpenGLFramebufferObject(req_size, format);
        fbo->bind();
//        glDevice.setFbo(fbo->handle());
        glViewport(0, 0, req_size.width(), req_size.height());
    }
    return true;
}

bool PixelBuffer::doneCurrent()
{
    if (invalid)
        return false;
    qctx->doneCurrent();
    return true;
}

QImage PixelBuffer::toImage() const
{
    if (invalid)
        return QImage();

    const_cast<PixelBuffer *>(this)->makeCurrent();
    if (fbo)
        fbo->bind();
    return qt_gl_read_frame_buffer(req_size, qctx->format().alpha(), true);
}

QGLFormat PixelBuffer::format() const
{
    return invalid ? QGLFormat() : qctx->format();
}
