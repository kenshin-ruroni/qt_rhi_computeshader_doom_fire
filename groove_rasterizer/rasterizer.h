#pragma once

#include <stdint.h>
#include <QVulkanInstance>
#include <QRhiWidget>

#include <QVector2D>
#include <QMatrix4x4>
#include <QFile>
#include <rhi/qrhi.h>


class Rasterizer : public QRhiWidget
{

    const uint32_t palette_fire[37] = {
        0x070707FF, 0x1F0707FF, 0x2F0F07FF, 0x470F07FF,
        0x571707FF, 0x671F07FF, 0x771F07FF, 0x8F2707FF,
        0x9F2F07FF, 0xAF3F07FF, 0xBF4707FF, 0xC74707FF,
        0xDF4F07FF, 0xDF5707FF, 0xDF570FFF, 0xD75F0FFF,
        0xD7671FFF, 0xCF6F1FFF, 0xCF771FFF, 0xCF7F24FF,
        0xC78724FF, 0xC78F2FFF, 0xC7972FFF, 0xBF9F2FFF,
        0xBFA72FFF, 0xBFAF2FFF, 0xB7AF2FFF, 0xB7B72FFF,
        0xB7B737FF, 0xCFCF6FFF, 0xDFDF9FFF, 0xEFEFC7FF,
        0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
        0xFFFFFFFF
    };

    Q_OBJECT


public:
    explicit Rasterizer(QWidget *parent = nullptr);

private:


    QShader getShader(const QString &resourcePath) {
        // 1. Ouverture du fichier binaire .qsb dans les ressources Qt
        QFile file(resourcePath);
        if (!file.open(QIODevice::ReadOnly)) {
            qWarning() << "Impossible d'ouvrir le fichier de shader :" << resourcePath;
            return QShader();
        }

        // 2. Lecture complète du contenu binaire
        QByteArray shaderData = file.readAll();

        // 3. Désérialisation du conteneur QShader
        QShader shader = QShader::fromSerialized(shaderData);

        // 4. Validation du shader chargé
        if (!shader.isValid()) {
            qWarning() << "Le fichier shader est invalide ou corrompu :" << resourcePath;
            return QShader();
        }

        return shader;
    }



    void setupComputePipeline();
    void setupVertexData();
    void createComputeShader();
    void setup_ssbo_buffer();
    void update_ssbo_buffer();

    QWindow *m_window;
    QRhi *m_rhi;

    QRhiTexture *m_outputTexture;
    QRhiComputePipeline *m_computePipeline;
    QRhiResourceUpdateBatch *m_resourceUpdates;
    QRhiBuffer *m_ssbo_buffer;
    QRhiShaderResourceBindings *m_compute_pass_bindings;

    QRhiShaderResourceBindings *m_render_to_texture_bindings;



    QRhiSwapChain *swapChain;
    QRhiRenderPassDescriptor *rp;

    QRhiBuffer *m_render_to_texture_vertex_buffer;
    QRhiBuffer *m_ubuf;
    QRhiShaderResourceBindings *m_srb;
    QRhiGraphicsPipeline *m_render_to_texture_pipeline;
    QRhiShaderResourceBindings *m_render_to_texture_shader_bindings;

    // Triangle strip data
    struct Vertex {
        float x, y;
    };

    QVector<Vertex> m_vertices;
    QVector<uint32_t> m_indices;
    QMatrix4x4 m_projectionMatrix;

    QRhiTexture *compute_texture_out;

    QRhiTexture *m_rtTexture;

    QRhiTextureRenderTarget *customRenderTarget;

    QSize size ;

    quint64 m_frameCounter = 0;

    QImage rgbaImg;

    protected:

    void initialize(QRhiCommandBuffer *cb) override
    {


        setupVertexData();


        m_window = (QWindow *)parentWidget()->parentWidget();

         m_rhi = rhi();
        if (!m_rhi) {
            qWarning() << "Failed to initialize QRHI";
            return;
        }

        swapChain =  m_rhi->newSwapChain();

        QRhiRenderPassDescriptor *rp = swapChain->newCompatibleRenderPassDescriptor();
        swapChain->setRenderPassDescriptor(rp);

        setupVertexData();


        size = renderTarget()->pixelSize();

        // Create ssbo buffer for triangle data
        m_ssbo_buffer = m_rhi->newBuffer(QRhiBuffer::Dynamic,
                                         QRhiBuffer::StorageBuffer,
                                         m_vertices.size() * sizeof(Vertex));

        if (!m_ssbo_buffer->create()) {
            qWarning() << "Failed to create ssbo buffer";
        }

        update_ssbo_buffer();

        compute_texture_out = m_rhi->newTexture(
            QRhiTexture::RGBA8,               // Format (matches standard QImage/PNG raw data)
            size,                // Size of the texture
            1,                                // Sample count (1 for non-MSAA)
            QRhiTexture::UsedWithLoadStore | QRhiTexture::UsedAsTransferSource // Usage flag
            );
        if ( !compute_texture_out->create())
        {
            qFatal()<<"unable to create compute texture out ";
        }

        m_rtTexture = m_rhi->newTexture(QRhiTexture::RGBA8, size, 1,
                                      QRhiTexture::RenderTarget | QRhiTexture::UsedAsTransferSource );
        m_rtTexture->create();

        QRhiTextureRenderTargetDescription rtDesc(m_rtTexture);
        customRenderTarget = m_rhi->newTextureRenderTarget(rtDesc);
        if (customRenderTarget->create() == false )
        {
            qFatal()<<"unable to create custom render target ";
        }

        // Create resource bindings
        m_compute_pass_bindings = m_rhi->newShaderResourceBindings();
        m_compute_pass_bindings->setBindings(
        {
            {
                QRhiShaderResourceBinding::bufferStore(0, QRhiShaderResourceBinding::ComputeStage,m_ssbo_buffer )
            },
             {
                 // Binding 0: Image storage for compute writing
                 QRhiShaderResourceBinding::imageLoadStore(
                     5,                                      // Binding index in GLSL
                     QRhiShaderResourceBinding::ComputeStage, // Shader stage
                     compute_texture_out,                          // The QRhiTexture
                     0
                     )
             }
        });


        m_compute_pass_bindings->create();

        m_computePipeline = m_rhi->newComputePipeline();

        QShader compute_shader = getShader(QStringLiteral(":/shaders/rasterize.comp.qsb"));

        m_computePipeline->setShaderStage(
        {
            QRhiShaderStage::Compute,
           compute_shader
        });         // Associe le shader
        m_computePipeline->setShaderResourceBindings(m_compute_pass_bindings);     // Associe les bindings obligatoires

        if (!m_computePipeline->create())
        {
            qFatal("unable to create compute pipeline. ");
            return;
        }

        // render compute shader results to a texture (2nde pass)

        // vertex/fragment rendering on full quad

        struct vertex_quad {
            float position[2]; // x, y (layout location = 0)
            float texCoord[2]; // u, v (layout location = 1)
        };

        const vertex_quad quadVertices[] = {
            // Triangle 1
            { { -1.0f,  1.0f }, { 0.0f, 0.0f } }, // Top-Left
            { { -1.0f, -1.0f }, { 0.0f, 1.0f } }, // Bottom-Left
            { {  1.0f, -1.0f }, { 1.0f, 1.0f } }, // Bottom-Right

            // Triangle 2
            { { -1.0f,  1.0f }, { 0.0f, 0.0f } }, // Top-Left
            { {  1.0f, -1.0f }, { 1.0f, 1.0f } }, // Bottom-Right
            { {  1.0f,  1.0f }, { 1.0f, 0.0f } }  // Top-Right
        };

        quint32 size_quad = sizeof(quadVertices);
        m_render_to_texture_vertex_buffer = m_rhi->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer, size_quad);
        m_render_to_texture_vertex_buffer->create();

        QShader vertex_shader = getShader(QStringLiteral(":/shaders/render_to_target.vert.qsb"));
        QShader fragment_shader = getShader(QStringLiteral(":/shaders/render_to_target.frag.qsb"));

        m_render_to_texture_pipeline = m_rhi->newGraphicsPipeline();
        m_render_to_texture_pipeline->setShaderStages(
            {
                { QRhiShaderStage::Vertex,
                  vertex_shader
                },
                { QRhiShaderStage::Fragment,
                  fragment_shader
                }
        });

        QRhiVertexInputLayout inputLayout;
        inputLayout.setBindings({
            { 4 * sizeof(float) }
        });
        size_t o1 = offsetof(vertex_quad, position);
        size_t o2 = offsetof(vertex_quad, texCoord);
        inputLayout.setAttributes({
            { 0, 0, QRhiVertexInputAttribute::Float2, offsetof(vertex_quad, position) }, // position (loc 0)
            { 0, 1, QRhiVertexInputAttribute::Float2, offsetof(vertex_quad, texCoord) }  // texCoord (loc 1)
        });

        // Create the bindings object
        m_render_to_texture_shader_bindings = m_rhi->newShaderResourceBindings();

        QRhiSampler *sampler = m_rhi->newSampler(
            QRhiSampler::Linear,              // Magnification filter (when zooming in)
            QRhiSampler::Linear,              // Minification filter (when zooming out)
            QRhiSampler::None,                // Mipmap filtering mode
            QRhiSampler::ClampToEdge,         // Horizontal wrap mode (U axis)
            QRhiSampler::ClampToEdge          // Vertical wrap mode (V axis)
            );

        if( !sampler->create() )
        {
            qFatal()<<"unable to create sampler2D";
        }

        // Configure the layout bindings
        m_render_to_texture_shader_bindings->setBindings({
            QRhiShaderResourceBinding::sampledTexture(
                2,                                    // Binding slot matching the shader
                QRhiShaderResourceBinding::FragmentStage,  // Visibility stage
                compute_texture_out,                              // Your QRhiTexture*
                sampler                               // Your QRhiSampler*
                )
        });

        // Allocate and build backend resources
        if ( m_render_to_texture_shader_bindings->create() == false )
        {
            qFatal()<<"unable to create texture shader ";
        }

        m_render_to_texture_pipeline->setShaderResourceBindings(m_render_to_texture_shader_bindings);
        m_render_to_texture_pipeline->setVertexInputLayout(inputLayout);
        m_render_to_texture_pipeline->setRenderPassDescriptor( renderTarget()->renderPassDescriptor());
        m_render_to_texture_pipeline->setCullMode(QRhiGraphicsPipeline::None);
        m_render_to_texture_pipeline->setTopology(QRhiGraphicsPipeline::Triangles);
        if ( m_render_to_texture_pipeline->create() == false )
        {
            qFatal()<<"unable to create texture pipeline ";
        }

        QRhiResourceUpdateBatch *resourceUpdates = m_rhi->nextResourceUpdateBatch();
        resourceUpdates->uploadStaticBuffer(m_render_to_texture_vertex_buffer, quadVertices);
        cb->resourceUpdate(resourceUpdates);

        /*
        QImage img(QStringLiteral("wallpaperflare.com_wallpaper.jpg"));
        if (img.isNull()) {
            qWarning() << "Impossible de charger le fichier PNG !";
            return;
        }
        rgbaImg = img.convertToFormat(QImage::Format_RGBA8888_Premultiplied).scaled(size.rwidth(),size.rheight(),Qt::AspectRatioMode::KeepAspectRatio).flipped();
        // QRhi nécessite un format de pixels standardisé.
        //convertToFormat(QImage::Format_RGBA8888) garantit la compatibilité avec QRhiTexture::RGBA8


        rgbaImg.save("rscaled.png");*/

    }

    QRhiReadbackResult rbResult;

    void render(QRhiCommandBuffer *cb) override
    {

        QRhiResourceUpdateBatch *computeBatch = m_rhi->nextResourceUpdateBatch();
        // Update uniform buffer with new data


            cb->beginComputePass(computeBatch);

           // computeBatch->uploadTexture(compute_texture_out, rgbaImg);

            // Execute compute shader
            cb->setComputePipeline(m_computePipeline);

            cb->setShaderResources(m_compute_pass_bindings);

            cb->dispatch(size.rwidth()/32, size.rheight()/32,1);

            QRhiReadbackDescription rb(compute_texture_out);


         //   computeBatch->readBackTexture(rb, &rbResult);

            cb->endComputePass(computeBatch);

         /*    rbResult.completed = [this]() {
                if (!rbResult.data.isEmpty()) {
                    // Les données brutes sont dans result->data (QByteArray)
                    const uchar *pixels = reinterpret_cast<const uchar *>(rbResult.data.constData());
                    int width = rbResult.pixelSize.width();
                    int height = rbResult.pixelSize.height();

                    // Exemple : conversion immédiate en QImage (adapter le format selon la texture)
                    QImage image(pixels, width, height, QImage::Format_RGBA8888);
                    bool s = image.save("image_gpu.png");
                    int i = 0;
                }
            };
    */

            QRhiResourceUpdateBatch *nextBatch = m_rhi->nextResourceUpdateBatch();

            // Clear output texture (if needed)
            QRhiRenderTarget *rt = renderTarget();
            const QColor clearColor = QColor::fromRgbF(1.f, 0.f,0.f,1.f );
            QSize currentPixelSize = rt->pixelSize();
            // Déclaration stricte du Viewport via le constructeur QRhiViewport
            cb->setViewport(QRhiViewport(0.0f, 0.0f, float(currentPixelSize.width()), float(currentPixelSize.height())));

            cb->beginPass( rt, clearColor, { 1.0f, 0 },nextBatch);


           // nextBatch->uploadTexture(compute_texture_out, rgbaImg);

            cb->setGraphicsPipeline(m_render_to_texture_pipeline);
            cb->setShaderResources(m_render_to_texture_shader_bindings);
            const QRhiCommandBuffer::VertexInput vertexBinding(m_render_to_texture_vertex_buffer, 0);
            cb->setVertexInput(0, 1, &vertexBinding);
            // 5. Exécution du dessin (6 sommets pré-chargés)
            cb->draw(6);



          cb->endPass();






          update();
    }
};

