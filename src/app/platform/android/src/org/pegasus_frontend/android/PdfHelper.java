// Pegasus Frontend
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.


package org.pegasus_frontend.android;

import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.pdf.PdfRenderer;
import android.os.ParcelFileDescriptor;
import java.io.File;


// Renders PDF pages with the system PDF renderer (PDFium, part of Android
// since API 21), so themes can show game manuals without any extra library.
// One document is kept open, as page turns usually stay within one manual.
// PdfRenderer is not thread safe, hence the synchronized entry points.
public class PdfHelper {
    private static final int MAX_PIXELS = 4096 * 4096;
    private static final int DEFAULT_HEIGHT = 1600;

    private static String s_path = null;
    private static ParcelFileDescriptor s_fd = null;
    private static PdfRenderer s_renderer = null;


    private static void close() {
        try {
            if (s_renderer != null)
                s_renderer.close();
            if (s_fd != null)
                s_fd.close();
        } catch (Exception e) {
            e.printStackTrace();
        }
        s_renderer = null;
        s_fd = null;
        s_path = null;
    }

    private static boolean open(String path) {
        if (s_renderer != null && path.equals(s_path))
            return true;

        close();
        try {
            s_fd = ParcelFileDescriptor.open(new File(path), ParcelFileDescriptor.MODE_READ_ONLY);
            s_renderer = new PdfRenderer(s_fd);
            s_path = path;
            return true;
        } catch (Exception e) {
            e.printStackTrace();
            close();
            return false;
        }
    }


    public static synchronized int pageCount(String path) {
        return open(path) ? s_renderer.getPageCount() : 0;
    }

    // Returns { width, height, pixel0, pixel1, ... } as 0xAARRGGBB, or null.
    // The page is scaled to fit maxWidth x maxHeight (0 = unbounded), keeping
    // its aspect ratio, on a white background.
    public static synchronized int[] renderPage(String path, int index, int maxWidth, int maxHeight) {
        if (!open(path))
            return null;
        if (index < 0 || index >= s_renderer.getPageCount())
            return null;

        PdfRenderer.Page page = null;
        try {
            page = s_renderer.openPage(index);

            final float page_w = Math.max(1, page.getWidth());
            final float page_h = Math.max(1, page.getHeight());
            if (maxWidth <= 0 && maxHeight <= 0)
                maxHeight = DEFAULT_HEIGHT;

            float scale = Float.MAX_VALUE;
            if (maxWidth > 0)
                scale = Math.min(scale, maxWidth / page_w);
            if (maxHeight > 0)
                scale = Math.min(scale, maxHeight / page_h);

            int w = Math.max(1, Math.round(page_w * scale));
            int h = Math.max(1, Math.round(page_h * scale));
            if ((long) w * h > MAX_PIXELS) {
                final double shrink = Math.sqrt((double) MAX_PIXELS / ((long) w * h));
                w = Math.max(1, (int) (w * shrink));
                h = Math.max(1, (int) (h * shrink));
            }

            final Bitmap bitmap = Bitmap.createBitmap(w, h, Bitmap.Config.ARGB_8888);
            bitmap.eraseColor(Color.WHITE);
            page.render(bitmap, null, null, PdfRenderer.Page.RENDER_MODE_FOR_DISPLAY);

            final int[] out = new int[2 + w * h];
            out[0] = w;
            out[1] = h;
            bitmap.getPixels(out, 2, w, 0, 0, w, h);
            bitmap.recycle();
            return out;
        } catch (Exception e) {
            e.printStackTrace();
            return null;
        } finally {
            if (page != null)
                page.close();
        }
    }
}
