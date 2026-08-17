#include <QApplication>
#include <QVulkanInstance>
#include <private/qrhivulkan_p.h>
#include <QQuickWindow>
#include <QTimer>
#include <QElapsedTimer>

 static QVulkanInstance inst;

#include "mainwindow.h"



int main(int argc, char *argv[])
{
    QApplication a(argc, argv);


    // Demander une configuration de surface standard propre pour Vulkan/RHI
    QSurfaceFormat format = QSurfaceFormat::defaultFormat();
    // Optionnel : Activer le support Alpha si vous faites de la transparence
    format.setAlphaBufferSize(8);
    QSurfaceFormat::setDefaultFormat(format);

    // Initialize QRHI

    //inst.setLayers({ "VK_LAYER_KHRONOS_validation" }); // for debugging only, not for release builds
    inst.setExtensions(QRhiVulkanInitParams::preferredInstanceExtensions());
    if (!inst.create())
        qFatal("Vulkan not available");

    MainWindow window;

    window.show();

    return a.exec();
}
