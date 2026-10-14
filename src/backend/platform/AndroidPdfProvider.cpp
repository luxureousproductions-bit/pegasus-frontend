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


#include "AndroidPdfProvider.h"

#include "AndroidHelpers.h"

#include <QtAndroidExtras/QAndroidJniEnvironment>
#include <QtAndroidExtras/QAndroidJniObject>


AndroidPdfProvider::AndroidPdfProvider()
    : QQuickImageProvider(QQuickImageProvider::Image, QQmlImageProviderBase::ForceAsynchronousImageLoading)
{}

QImage AndroidPdfProvider::requestImage(const QString& id, QSize* size, const QSize& requested_size)
{
    static constexpr auto JNI_CLASS = "org/pegasus_frontend/android/PdfHelper";
    static constexpr auto JNI_METHOD = "renderPage";
    static constexpr auto JNI_SIGNATURE = "(Ljava/lang/String;III)[I";

    // id: <page>/<percent-encoded path>
    const int sep = id.indexOf(QChar('/'));
    if (sep <= 0)
        return QImage();

    bool page_ok = false;
    const int page = id.leftRef(sep).toInt(&page_ok);
    const QString path = android::pdf_local_path(id.mid(sep + 1), true);
    if (!page_ok || page < 0 || path.isEmpty())
        return QImage();

    QAndroidJniEnvironment jni_env;
    const auto jni_path = QAndroidJniObject::fromString(path);
    const auto jni_result = QAndroidJniObject::callStaticObjectMethod(
        JNI_CLASS, JNI_METHOD, JNI_SIGNATURE,
        jni_path.object<jstring>(),
        static_cast<jint>(page),
        static_cast<jint>(requested_size.width()),
        static_cast<jint>(requested_size.height()));
    if (jni_env->ExceptionCheck()) {
        jni_env->ExceptionClear();
        return QImage();
    }

    const auto jni_arr = jni_result.object<jintArray>();
    if (!jni_arr)
        return QImage();

    const jsize len = jni_env->GetArrayLength(jni_arr);
    if (len < 2)
        return QImage();

    jint dims[2];
    jni_env->GetIntArrayRegion(jni_arr, 0, 2, dims);
    const int width = dims[0];
    const int height = dims[1];
    if (width <= 0 || height <= 0 || len != 2 + static_cast<jsize>(width) * height)
        return QImage();

    // Java ints are 0xAARRGGBB, the same layout as QImage's 32-bit formats.
    // The page is drawn on white, so the alpha channel can be ignored.
    QImage image(width, height, QImage::Format_RGB32);
    if (image.isNull())
        return QImage();

    for (int y = 0; y < height; y++) {
        jni_env->GetIntArrayRegion(jni_arr, 2 + y * width, width,
                                   reinterpret_cast<jint*>(image.scanLine(y)));
    }

    if (size)
        *size = image.size();
    return image;
}
