#pragma once

#include <cstdlib>
#include <cmath>

#include <random>
#include <vector>
#include <stdint.h>
#include <QVulkanInstance>
#include <QRhiWidget>
#include <QWindow>
#include <QVector2D>
#include <QMatrix4x4>
#include <QFile>
#include <rhi/qrhi.h>
#include <QPainter>

#define DEBUG_COMPUTE_SHADER_RESULT 0

class Rasterizer : public QRhiWidget
{

    std::vector< uint32_t > palettes = {
    // red fire

        0xFFFFFFFF, // Index 0 : L'unique case Blanc Pur tout à droite
        0xEFEFC7FF, // Index 1 : Crème / Ivoire très clair (2e case en partant de la droite)
        0xDFDF9FFF, // Index 2 : Jaune pastel très doux (3e case en partant de la droite)
        0xCFCF6FFF, // Index 3 : Jaune maïs / doré (4e case en partant de la droite)
        // Reste du dégradé qui continue vers le orange, rouge, puis noir
        0xB7B737FF, 0xB7B72FFF, 0xB7AF2FFF, 0xBFAF2FFF,
        0xBFA72FFF, 0xBF9F2FFF, 0xC7972FFF, 0xC78F2FFF,
        0xC78724FF, 0xCF7F24FF, 0xCF771FFF, 0xCF6F1FFF,
        0xD7671FFF, 0xD75F0FFF, 0xDF570FFF, 0xDF5707FF,
        0xDF4F07FF, 0xC74707FF, 0xBF4707FF, 0xAF3F07FF,
        0x9F2F07FF, 0x8F2707FF, 0x771F07FF, 0x671F07FF,
        0x571707FF, 0x470F07FF, 0x2F0F07FF, 0x1F0707FF,
        0x070707FF, 0x070707FF, 0x070707FF, 0x070707FF,
        0x070707FF,
     // green fire
        0xFFFFFFFF, // Index 0 : L'unique case Blanc Pur
        0xEFEFC7FF, 0xDFDF9FFF, 0xCFCF6FFF, 0xB7B737FF, // Descentes vers le jaune-vert clair
        0x97B72FFF, 0x77AF2FFF, 0x57AF2FFF, 0x37AF2FFF,
        0x1F9F2FFF, 0x0F972FFF, 0x078F2FFF, 0x078724FF, // Cœur de flamme vert électrique
        0x077F24FF, 0x07771FFF, 0x076F1FFF, 0x07671FFF,
        0x075F0FFF, 0x07570FFF, 0x075707FF, 0x074F07FF, // Dégradés vert sapin / émeraude
        0x074707FF, 0x073F07FF, 0x073F07FF, 0x072F07FF,
        0x072707FF, 0x071F07FF, 0x071F07FF, 0x071707FF, // Refroidissement sombre
        0x070F07FF, 0x070F07FF, 0x070707FF, 0x070707FF,
        0x070707FF, 0x070707FF, 0x070707FF, 0x070707FF,

        //purple fire
        0xFFFFFFFF, // Index 0 : L'unique case Blanc Pur
        0xF4EFE7FF, 0xECEFDFFF, 0xE0CFCFFF, 0xDCB7B7FF, // Transitions douces pastel
        0xDCB7B7FF, 0xD4AFB7FF, 0xD4AFBFFF, 0xCCA7BFFF,
        0xCC9FBFFF, 0xCC97C7FF, 0xC48FC7FF, 0xC487C7FF, // Magenta et violets électriques très vifs
        0xC47FCFFF, 0xC477CFFF, 0xBC6FCFFF, 0xBC67D7FF,
        0xBC5FD7FF, 0xBC57DFFF, 0xB457DFFF, 0xAC4FDFFF, // Indigos profonds à moyenne énergie
        0x9747C7FF, 0x9047BFFF, 0x823FAFFF, 0x732F9FFF,
        0x67278FFF, 0x541F77FF, 0x471F67FF, 0x3B1757FF, // Teintes pourpres nocturnes
        0x2E0F47FF, 0x1F0F2FFF, 0x14071FFF, 0x070707FF,
        0x070707FF, 0x070707FF, 0x070707FF, 0x070707FF,
      // blue fire
        0xFFFFFFFF, // Index 0 : L'unique case Blanc Pur
        0xC7EFEFFF, 0x9FDFDFFF, 0x6FCFCFFF, 0x37B7B7FF, // Descentes vers le cyan arctique éclatant
        0x2FB7B7FF, 0x2FAFB7FF, 0x2FAFBFFF, 0x2FA7BFFF,
        0x2F9FBFFF, 0x2F97C7FF, 0x2F8FC7FF, 0x2487C7FF, // Bleus néon intenses
        0x247FCFFF, 0x1F77CFFF, 0x1F6FCFFF, 0x1F67D7FF,
        0x0F5FD7FF, 0x0F57DFFF, 0x0F57DFFF, 0x0F4FDFFF, // Bleus saphir profonds
        0x0F47C7FF, 0x0F47BFFF, 0x0F3FAFFF, 0x072F9FFF,
        0x07278FFF, 0x071F77FF, 0x071F67FF, 0x071757FF, // Bleux cobalt de fin de combustion
        0x070F47FF, 0x070F2FFF, 0x07071FFF, 0x070707FF,
        0x070707FF, 0x070707FF, 0x070707FF, 0x070707FF,

};



    Q_OBJECT


public:
    explicit Rasterizer(QWidget *parent = nullptr);

private:


    std::random_device rd;

    // 2. Choisir un moteur de génération (Mersenne Twister 64-bit est excellent)
    std::mt19937 gen = std::mt19937(rd());

    size_t max_rnd = 4;
    // 3. Définir la distribution uniforme entre 0 et 3 inclus [0, 3]
    std::uniform_int_distribution<int> distrib = std::uniform_int_distribution<int>(0, max_rnd);

    /*
     * https://github.com/StanislavPetrovV/DOOM-Fire-Algorithm
     *
     */

    inline void spread_fire(size_t from) {

        uint32_t m = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX) * 6.f;

         uint32_t rand = static_cast<uint32_t>(std::round((static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX) ) * m )) & 7;

        fire_matrix[from] = (fire_matrix[from - fire_matrix_width] + rand  )  % 37 ;
    }

    inline void update_fire_matrix_old()
    {
        for(size_t x = 0 ; x < fire_matrix_width; ++x) {
            for (size_t y = 1; y < fire_matrix_height ; ++y) {
                spread_fire(4 + y * fire_matrix_width + x);
            }
        }

    }

    inline void update_fire_matrix()
    {
        for(size_t x = 0 ; x < fire_matrix_width; ++x)
        {
            for (size_t y = 1; y < fire_matrix_height ; ++y)
            {
                uint32_t color_index = fire_matrix[4 + x + fire_matrix_width * y];
                uint rnd = distrib(gen); // nombre entre 0 et 3

               // qDebug("%i ",(int) ( (x - rnd + 1) % fire_matrix_width ) );
                if ( color_index != 36)
                {
                    fire_matrix[ 4 + (x - rnd + 1) % fire_matrix_width +  fire_matrix_width * (y - 1) ] = (color_index - rnd % (max_rnd - 1)) % 37;
                }
               else{
                   fire_matrix[ 4 + x  +  fire_matrix_width * (y - 1) ] = 0;
               }
            }
        }

    }

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

    void createComputeShader();
    void setup_ssbo_buffer();
    void update_ssbo_buffer();

    QRhi *m_rhi;

    QRhiTexture *m_outputTexture;
    QRhiComputePipeline *m_computePipeline;
    QRhiResourceUpdateBatch *m_resourceUpdates;

    QRhiBuffer *fire_matrix_ssbo_buffer;

    QRhiBuffer *palettes_ssbo_buffer = nullptr;

    QRhiShaderResourceBindings *m_compute_pass_bindings;

    QRhiShaderResourceBindings *m_render_to_texture_bindings;

    QRhiTexture *doom_logo_texture = nullptr;

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

    size_t fire_matrix_width, fire_matrix_height;

    std::vector<uint32_t> fire_matrix; // contient un index de palette

    size_t fire_height = 37;

    size_t palette_index = 0;


    size_t scale_x = 4;
    size_t scale_y = 4;

    protected:

    void initialize(QRhiCommandBuffer *cb) override
    {

        // Récupération automatique et sécurisée de la taille réelle en pixels physiques
        QSize pixelSize = renderTarget()->pixelSize();

        fire_height = pixelSize.height();

        fire_matrix_width = pixelSize.width();
        fire_matrix_height = fire_height;

        // On réserve la taille : 2 emplacements (pour X et Y) + la taille du tableau 2D linéarisé
        fire_matrix.resize( 4 + fire_matrix_width * fire_height, 0);

        fire_matrix[0] = fire_matrix_width ;
        fire_matrix[1] = fire_matrix_height;
        fire_matrix[2] = scale_x;
        fire_matrix[3] = scale_y;


        for(size_t x = 0 ; x < fire_matrix_width; ++x)
        {
            fire_matrix[4 + x] = palette_index * fire_matrix_height;
        }

         m_rhi = rhi();
        if (!m_rhi) {
            qWarning() << "Failed to initialize QRHI";
            return;
        }

        swapChain =  m_rhi->newSwapChain();

        QRhiRenderPassDescriptor *rp = swapChain->newCompatibleRenderPassDescriptor();
        swapChain->setRenderPassDescriptor(rp);

        size = renderTarget()->pixelSize();

        // Create ssbo buffer for triangle data
        fire_matrix_ssbo_buffer = m_rhi->newBuffer(QRhiBuffer::Dynamic,
                                         QRhiBuffer::StorageBuffer,
                                         fire_matrix.size() * sizeof(uint32_t));

        if (!fire_matrix_ssbo_buffer->create()) {
            qWarning() << "Failed to create ssbo buffer";
        }

        // 2. Téléversement immédiat des données vers le GPU
        QRhiResourceUpdateBatch *initfireMatrixUpdates = m_rhi->nextResourceUpdateBatch();
        initfireMatrixUpdates->uploadStaticBuffer(fire_matrix_ssbo_buffer, fire_matrix.data());
        cb->resourceUpdate(initfireMatrixUpdates); // Soumission de la mise à jour

        // 1. Allocation du tampon sur le GPU (Mode statique car les palettes ne changent pas)
        palettes_ssbo_buffer = m_rhi->newBuffer(
            QRhiBuffer::Static,
            QRhiBuffer::StorageBuffer, // Type correspond au layout(std430) buffer du shader
            palettes.size() * sizeof(uint32_t)
            );

        if (!palettes_ssbo_buffer->create()) {
            qFatal("Impossible de créer le tampon de palettes RHI");
        }

        // 2. Téléversement immédiat des données vers le GPU
        QRhiResourceUpdateBatch *initUpdates = m_rhi->nextResourceUpdateBatch();
        initUpdates->uploadStaticBuffer(palettes_ssbo_buffer, palettes.data());
        cb->resourceUpdate(initUpdates); // Soumission de la mise à jour

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

        QImage img(":/images/doom_logo.png");
        img = img.convertToFormat(QImage::Format_RGBA8888); // Format correspondant à rgba8 en GLSL

        // 2. Allouer la texture QRhi
        doom_logo_texture = m_rhi->newTexture(
            QRhiTexture::RGBA8,
            img.size(),
            1,
            QRhiTexture::UsedWithLoadStore// <-- Obligatoire pour imageLoad/imageStore
            );

        if (!doom_logo_texture->create()) {
            qFatal("Impossible de créer la texture à partir du logo doom.");
        }

        // 3. Téléverser les pixels CPU vers la mémoire GPU
        QRhiResourceUpdateBatch *upload_doom_logo_Batch = m_rhi->nextResourceUpdateBatch();

        QRhiTextureSubresourceUploadDescription doom_logo_resource_updload_desc(img);

        QRhiTextureUploadDescription uploadDesc(QRhiTextureUploadEntry(0,0,doom_logo_resource_updload_desc));
        upload_doom_logo_Batch->uploadTexture(doom_logo_texture, uploadDesc);

        cb->resourceUpdate(upload_doom_logo_Batch); // Validation immédiate

        // Create resource bindings
        m_compute_pass_bindings = m_rhi->newShaderResourceBindings();
        m_compute_pass_bindings->setBindings(
        {
            {
                 QRhiShaderResourceBinding::bufferStore(0, QRhiShaderResourceBinding::ComputeStage, palettes_ssbo_buffer)
            },
            {
                QRhiShaderResourceBinding::bufferStore(1, QRhiShaderResourceBinding::ComputeStage,fire_matrix_ssbo_buffer )
            },
             {
                 // Binding 0: Image storage for compute writing
                 QRhiShaderResourceBinding::imageLoadStore(
                     5,                                      // Binding index in GLSL
                     QRhiShaderResourceBinding::ComputeStage, // Shader stage
                     compute_texture_out,                          // The QRhiTexture
                     0
                     )
             },
             {
                 QRhiShaderResourceBinding::imageLoadStore(
                     6,                                      // Correspond au binding = 6 en GLSL
                     QRhiShaderResourceBinding::ComputeStage, // Visible par le Compute Shader
                     doom_logo_texture,                         // doom logo as image2d
                     0                                       // Niveau de Mipmap (0 = base)
                     )
             }
        });


        if ( !m_compute_pass_bindings->create() )
        {
            qFatal("Impossible de créer la compute shader pass.");
        }

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


    }

#if DEBUG_COMPUTE_SHADER_RESULT == 1
    QRhiReadbackResult rbResult;
#endif
    void render(QRhiCommandBuffer *cb) override
    {

        QRhiResourceUpdateBatch *computeBatch = m_rhi->nextResourceUpdateBatch();
        // Update uniform buffer with new data


            cb->beginComputePass(computeBatch);

           // computeBatch->uploadTexture(compute_texture_out, rgbaImg);

            // Execute compute shader
            cb->setComputePipeline(m_computePipeline);

            cb->setShaderResources(m_compute_pass_bindings);

            uint32_t groupX = (size.rwidth() + 31) / 32;
            uint32_t groupY = (size.rheight() + 31) / 32;
            cb->dispatch(groupX, groupY, 1);

            QRhiReadbackDescription rb(compute_texture_out);

#if DEBUG_COMPUTE_SHADER_RESULT == 1
           computeBatch->readBackTexture(rb, &rbResult);
#endif
            cb->endComputePass(computeBatch);

#if DEBUG_COMPUTE_SHADER_RESULT == 1
            rbResult.completed = [this]() {
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
#endif

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

          update_fire_matrix();
          // 2. Téléversement immédiat des données vers le GPU
          QRhiResourceUpdateBatch *updatefireMatrixUpdates = m_rhi->nextResourceUpdateBatch();
          updatefireMatrixUpdates->updateDynamicBuffer(fire_matrix_ssbo_buffer,0,fire_matrix.size() * sizeof(uint32_t), fire_matrix.data());
          cb->resourceUpdate(updatefireMatrixUpdates); // Soumission de la mise à jour

    }
};

