#pragma once

#include <QVulkanInstance>
#include <QRhiWidget>

#include <QVector2D>
#include <QMatrix4x4>
#include <QFile>
#include <rhi/qrhi.h>


class Rasterizer : public QRhiWidget
{
    Q_OBJECT

    const char *vertex_shader = R"(

        #version 430 core

        // Input vertex attributes from your vertex buffer
        layout(location = 0) in vec2 inPosition;
        layout(location = 1) in vec2 inTexCoord;

        // Output to the fragment shader
        layout(location = 0) out vec2 vTexCoord;

        void main() {
            // Pass the texture coordinates to the fragment shader
            vTexCoord = inTexCoord;

            // Output standard clip-space position (Z=0.0, W=1.0)
            gl_Position = vec4(inPosition, 0.0, 1.0);
        }

)";


    const char *frag_shader = R"(

            // Input from the vertex shader
            layout(location = 0) in vec2 vTexCoord;

            // Output color to the frame buffer
            layout(location = 0) out vec4 fragColor;

            // Texture and sampler binding (set 0, binding 1)
            layout(set = 0, binding = 1) uniform sampler2D texSampler;

            void main() {
                // Sample the color from the texture using the coordinates
                fragColor = texture(texSampler, vTexCoord);
            }

)";


    const char* slang_compute_shader = R"(
#version 430 core

// Input: triangle strip data
layout(std430, binding = 0) readonly buffer Triangles { vec2 vertices[]; };
layout(std430, binding = 1) readonly buffer Indices { uint indices[]; };

// Output: rendered image
layout(std430, binding = 2) writeonly buffer OutputImage { uint pixels[]; };

// Uniforms for rendering configuration
layout(push_constant) uniform RenderParams {
    uint triangleCount;
    uint width;
    uint height;
    vec4 clearColor;
} params;

// Shared memory for fragment processing
shared vec2 sharedVertices[3];

// Main compute kernel
void main() {
    uint tid = gl_GlobalInvocationID.x;
    uint totalPixels = params.width * params.height;

    if (tid >= totalPixels) return;

    // Convert linear pixel index to 2D coordinates
    uint x = tid % params.width;
    uint y = tid / params.width;

    // Initialize pixel with clear color
    pixels[tid] = uint(params.clearColor.r * 255.0) |
                  (uint(params.clearColor.g * 255.0) << 8) |
                  (uint(params.clearColor.b * 255.0) << 16) |
                  (uint(params.clearColor.a * 255.0) << 24);

    // Early exit if we're outside triangle bounds
    if (params.triangleCount == 0) return;

    // Process triangle strips
    for (uint i = 0; i < params.triangleCount - 2; i++) {
        uint baseIndex = i * 3;

        // Load triangle vertices
        vec2 v0 = vertices[indices[baseIndex]];
        vec2 v1 = vertices[indices[baseIndex + 1]];
        vec2 v2 = vertices[indices[baseIndex + 2]];

        // Simple barycentric coordinate calculation
        vec2 p = vec2(float(x), float(y));

        // Compute barycentric coordinates
        vec2 e0 = v1 - v0;
        vec2 e1 = v2 - v0;
        vec2 d = p - v0;

        float dot00 = dot(e0, e0);
        float dot01 = dot(e0, e1);
        float dot02 = dot(e0, d);
        float dot11 = dot(e1, e1);
        float dot12 = dot(e1, d);

        float invDenom = 1.0 / (dot00 * dot11 - dot01 * dot01);
        float u = (dot11 * dot02 - dot01 * dot12) * invDenom;
        float v = (dot00 * dot12 - dot01 * dot02) * invDenom;

        // Check if point is inside triangle
        if ((u >= 0.0) && (v >= 0.0) && (u + v <= 1.0)) {
            // Simple color interpolation or flat shading
            vec3 triangleColor = vec3(0.5, 0.8, 1.0); // Cool blue

            // Convert color to packed uint format
            uint color = uint(triangleColor.r * 255.0) |
                         (uint(triangleColor.g * 255.0) << 8) |
                         (uint(triangleColor.b * 255.0) << 16) |
                         0xFF000000; // Alpha = 255

            pixels[tid] = color;

            // Early exit once we find a triangle
            break;
        }
    }
}
)";


    // Compute shader source code
    const char *computeShaderSource = R"(
    #version 450 core

    layout(local_size_x = 16, local_size_y = 16) in;

    // Output texture dimensions
    const uint OUTPUT_WIDTH = 800;
    const uint OUTPUT_HEIGHT = 600;

    // Uniform buffer for triangle data
    layout(std430, binding = 0) restrict readonly buffer TriangleBuffer {
        float vertices[];
    };

    // Output image
    layout(rgba8, binding = 1) writeonly uniform image2D outputImage;

    // Triangle rasterization parameters
    struct Triangle {
        vec2 v0, v1, v2;
        uint color;
    };

    void main() {
        // Calculate pixel coordinates
        uint x = gl_GlobalInvocationID.x;
        uint y = gl_GlobalInvocationID.y;

        if (x >= OUTPUT_WIDTH || y >= OUTPUT_HEIGHT) return;

        // Simple triangle rasterization using barycentric coordinates
        // This is a simplified example - in practice you'd use more sophisticated methods

        // Sample triangle data from buffer (this is just an example)
        uint triIndex = 0; // Simplified - in real app, you'd have multiple triangles
        uint vertexOffset = triIndex * 6; // 3 vertices * 2 floats each

        vec2 v0 = vec2(vertices[vertexOffset], vertices[vertexOffset + 1]);
        vec2 v1 = vec2(vertices[vertexOffset + 2], vertices[vertexOffset + 3]);
        vec2 v2 = vec2(vertices[vertexOffset + 4], vertices[vertexOffset + 5]);

        // Barycentric coordinates
        vec2 p = vec2(x, y);

        vec2 e0 = v1 - v0;
        vec2 e1 = v2 - v1;
        vec2 e2 = v0 - v2;

        vec2 v0p = p - v0;
        vec2 v1p = p - v1;
        vec2 v2p = p - v2;

        float area = abs(e0.x * e1.y - e1.x * e0.y);

        // Calculate barycentric coordinates
        float w0 = abs(e1.x * v0p.y - e1.y * v0p.x) / area;
        float w1 = abs(e2.x * v1p.y - e2.y * v1p.x) / area;
        float w2 = abs(e0.x * v2p.y - e0.y * v2p.x) / area;

        // Check if point is inside triangle
        if (w0 + w1 + w2 < 1.01f && w0 > 0.0f && w1 > 0.0f && w2 > 0.0f) {
            // Inside triangle - set color (red for demo)
            vec4 color = vec4(1.0, 0.0, 0.0, 1.0);
            imageStore(outputImage, ivec2(x, y), color);
        } else {
            // Outside - set transparent
            imageStore(outputImage, ivec2(x, y), vec4(0.0));
        }
    }
)";


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
            QRhiTexture::UsedWithLoadStore // | QRhiTexture::UsedAsTransferSource // Usage flag
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
        customRenderTarget->create();

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
                     0                                       // Mip level
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
            { 2 * sizeof(float) } ,
            { 2 * sizeof(float) }// Binding 0 stride
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
                0,                                    // Binding slot matching the shader
                QRhiShaderResourceBinding::FragmentStage,  // Visibility stage
                compute_texture_out,                              // Your QRhiTexture*
                sampler                               // Your QRhiSampler*
                )
        });

        // Allocate and build backend resources
        m_render_to_texture_shader_bindings->create();

        m_render_to_texture_pipeline->setShaderResourceBindings(m_render_to_texture_shader_bindings);
        m_render_to_texture_pipeline->setVertexInputLayout(inputLayout);
        m_render_to_texture_pipeline->setRenderPassDescriptor( renderTarget()->renderPassDescriptor());

        m_render_to_texture_pipeline->create();

        QRhiResourceUpdateBatch *resourceUpdates = m_rhi->nextResourceUpdateBatch();
        resourceUpdates->uploadStaticBuffer(m_render_to_texture_vertex_buffer, quadVertices);
        cb->resourceUpdate(resourceUpdates);

        QImage img(QStringLiteral("wallpaperflare.com_wallpaper.jpg"));
        if (img.isNull()) {
            qWarning() << "Impossible de charger le fichier PNG !";
            return;
        }
        rgbaImg = img.convertToFormat(QImage::Format_RGBA8888_Premultiplied).scaled(size.rwidth(),size.rheight(),Qt::AspectRatioMode::KeepAspectRatio).flipped();
        // QRhi nécessite un format de pixels standardisé.
        //convertToFormat(QImage::Format_RGBA8888) garantit la compatibilité avec QRhiTexture::RGBA8


        rgbaImg.save("rscaled.png");

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

            cb->dispatch(size.rwidth()/8, size.rheight()/8,1);

           // QRhiReadbackDescription rb(compute_texture_out);


          //  computeBatch->readBackTexture(rb, &rbResult);

            cb->endComputePass(computeBatch);

            /* rbResult.completed = [this]() {
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
            }; */


            QRhiResourceUpdateBatch *nextBatch = m_rhi->nextResourceUpdateBatch();

            // Clear output texture (if needed)
            QRhiRenderTarget *rt = renderTarget();
            const QColor clearColor = QColor::fromRgbF(0.f, 1.f,0,1.f );
            cb->setViewport({0.f,0.f,float(size.rwidth()), float(size.rheight())});
            cb->beginPass( rt, clearColor, { 1.0f, 0 },nextBatch);


           // nextBatch->uploadTexture(compute_texture_out, rgbaImg);

            cb->setGraphicsPipeline(m_render_to_texture_pipeline);
            cb->setShaderResources(m_render_to_texture_shader_bindings);

            // 5. Exécution du dessin (6 sommets pré-chargés)
            cb->draw(6);



          cb->endPass(nextBatch);






          update();
    }
};

