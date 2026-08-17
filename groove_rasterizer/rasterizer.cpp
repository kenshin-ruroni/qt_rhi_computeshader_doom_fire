
#include "rasterizer.h"
#include <QElapsedTimer>
#include <QDebug>


Rasterizer::Rasterizer(QWidget *parent)
    : QRhiWidget(parent)
    , m_window( (QWindow *) parent)
    , m_rhi(nullptr)
    , m_outputTexture(nullptr)
    , m_computePipeline(nullptr)
    , m_resourceUpdates(nullptr)
    , m_ssbo_buffer(nullptr)
    , m_compute_pass_bindings(nullptr)
{
}


void Rasterizer::setupVertexData()
{
    // Create triangle strip data (example with 4 vertices forming a quad)
    m_vertices.clear();

    // Quad vertices (2 triangles in strip)
    m_vertices.append({-0.5f, -0.5f});
    m_vertices.append({ 0.5f, -0.5f});
    m_vertices.append({-0.5f,  0.5f});
    m_vertices.append({ 0.5f,  0.5f});

    // Indices for triangle strip (3 indices per triangle)
    m_indices.clear();
    m_indices.append(0); m_indices.append(1); m_indices.append(2);
    m_indices.append(1); m_indices.append(3); m_indices.append(2);
}

void Rasterizer::update_ssbo_buffer()
{
    if (!m_ssbo_buffer || !m_rhi) return;

    // Prepare vertex data for GPU
    QVector<float> vertexData;
    for (const auto &v : m_vertices) {
        vertexData.append(v.x);
        vertexData.append(v.y);
    }

    // Update buffer
    m_resourceUpdates = m_rhi->nextResourceUpdateBatch();
    m_resourceUpdates->uploadStaticBuffer(m_ssbo_buffer, 0,vertexData.size() * sizeof(float), (const void *)vertexData.data());

}

