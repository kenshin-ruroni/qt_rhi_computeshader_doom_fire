
#include "rasterizer.h"
#include <QElapsedTimer>
#include <QDebug>


Rasterizer::Rasterizer(QWidget *parent)
    : QRhiWidget(parent)
    , m_rhi(nullptr)
    , m_outputTexture(nullptr)
    , m_computePipeline(nullptr)
    , m_resourceUpdates(nullptr)
    , fire_matrix_ssbo_buffer(nullptr)
    , m_compute_pass_bindings(nullptr)
{
}

void Rasterizer::update_ssbo_buffer()
{
    if (!fire_matrix_ssbo_buffer || !m_rhi) return;

    // Prepare vertex data for GPU
    QVector<float> vertexData;
    for (const auto &v : m_vertices) {
        vertexData.append(v.x);
        vertexData.append(v.y);
    }

    // Update buffer
    m_resourceUpdates = m_rhi->nextResourceUpdateBatch();
    m_resourceUpdates->uploadStaticBuffer(fire_matrix_ssbo_buffer, 0,vertexData.size() * sizeof(float), (const void *)vertexData.data());

}

