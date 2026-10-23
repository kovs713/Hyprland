#include "ScreenshareManager.hpp"
#include "../../Compositor.hpp"
#include "../../render/Renderer.hpp"
#include "../../output/Monitor.hpp"
#include "../../desktop/state/FocusState.hpp"
#include "../../desktop/state/FadingOutState.hpp"
#include "../../render/pass/RectPassElement.hpp"
#include "../../helpers/memory/Memory.hpp"
#include "../../debug/log/Logger.hpp"
#include "../../errorOverlay/Overlay.hpp"
#include "../../notification/NotificationOverlay.hpp"

#include <algorithm>

using namespace Screenshare;

// All omit-from-capture logic lives in this TU on purpose: upstream rewrites
// ScreenshareFrame.cpp regularly, and keeping the feature here shrinks every
// future merge down to the 3-line hook in renderMonitor().
bool CScreenshareFrame::needsCleanMonitor() const {
    if (m_session->m_type != SHARE_MONITOR && m_session->m_type != SHARE_REGION)
        return false;

    const auto monitor = m_session->monitor();
    if (!monitor)
        return false;

    for (const auto& layerLevel : monitor->m_layerSurfaceLayers) {
        for (const auto& weakLayer : layerLevel) {
            const auto layer = weakLayer.lock();
            if (layer && layer->mapped() && layer->m_ruleApplicator->omitsFromScreenShare())
                return true;
        }
    }

    return std::ranges::any_of(Desktop::fadingOutState()->fadeouts(),
                               [monitor](const auto& fadeout) { return fadeout && fadeout->monitor() == monitor && fadeout->omitFromScreenShare(); });
}

bool CScreenshareFrame::renderCleanMonitor(Render::CRenderContext& ctx, PHLMONITOR monitor) {
    const bool NEEDS_CLEAN_CAPTURE = needsCleanMonitor();
    if (!NEEDS_CLEAN_CAPTURE) {
        if (m_session->m_cleanCaptureOmissionActive)
            LOG(Log::DEBUG, "[clean-capture] No omitted layers remain; returning to the mirror framebuffer");

        m_session->m_cleanCaptureOmissionActive = false;
        m_session->m_cleanCaptureFallbackLogged = false;
        return false;
    }

    if (!m_session->m_cleanCaptureOmissionActive) {
        LOG(Log::DEBUG, "[clean-capture] Omitted layer detected on monitor {} at {}", monitor->m_name, monitor->m_pixelSize);
        m_session->m_cleanCaptureOmissionActive = true;
    }

    if (m_session->m_type != SHARE_MONITOR) {
        if (m_session->m_type == SHARE_REGION && !m_session->m_cleanCaptureFallbackLogged) {
            LOG(Log::DEBUG, "[clean-capture] Region capture is not supported in Stage 2; using black fallback");
            m_session->m_cleanCaptureFallbackLogged = true;
        }
        return false;
    }

    if (monitor->m_transform != WL_OUTPUT_TRANSFORM_NORMAL) {
        if (!m_session->m_cleanCaptureFallbackLogged)
            LOG(Log::ERR, "[clean-capture] Output transform {} is not supported yet; using black fallback", sc<int>(monitor->m_transform));
        m_session->m_cleanCaptureFallbackLogged = true;
        return false;
    }

    if (m_bufferSize != monitor->m_pixelSize) {
        if (!m_session->m_cleanCaptureFallbackLogged)
            LOG(Log::ERR, "[clean-capture] Buffer size {} does not match output size {}; using black fallback", m_bufferSize, monitor->m_pixelSize);
        m_session->m_cleanCaptureFallbackLogged = true;
        return false;
    }

    if (g_pSessionLockManager->isSessionLocked()) {
        if (!m_session->m_cleanCaptureFallbackLogged)
            LOG(Log::WARN, "[clean-capture] Session is locked; using black fallback");
        m_session->m_cleanCaptureFallbackLogged = true;
        return false;
    }

    if (!ctx.readOnlyEffects()) {
        if (!m_session->m_cleanCaptureFallbackLogged)
            LOG(Log::ERR, "[clean-capture] Isolated render setup failed; using black fallback");
        m_session->m_cleanCaptureFallbackLogged = true;
        return false;
    }

    m_session->m_cleanCaptureFallbackLogged = false;

    // Context flags survive pass execution and reset with endRender/abortRender.
    m_cleanCaptureRendered      = true;
    ctx.m_blockSurfaceFeedback  = true;
    ctx.m_renderingCapture      = true;
    ctx.m_renderingCleanCapture = true;

    const auto NOW       = Time::steadyNow();
    const auto RENDERBOX = CBox{0, 0, sc<int>(monitor->m_pixelSize.x), sc<int>(monitor->m_pixelSize.y)};

    g_pHyprRenderer->startRenderPass(ctx);
    g_pHyprRenderer->renderWorkspace(ctx, monitor, monitor->m_activeWorkspace, NOW, RENDERBOX);

    if (monitor == Desktop::focusState()->monitor()) {
        Notification::overlay()->draw(ctx, monitor);
        ErrorOverlay::overlay()->draw(ctx);
    }

    if (monitor->m_dpmsBlackOpacity->value() != 0.F) {
        g_pHyprRenderer->addPassElement(ctx,
                                        makeUnique<CRectPassElement>(CRectPassElement::SRectData{
                                            .box   = RENDERBOX,
                                            .color = Colors::BLACK.modifyA(monitor->m_dpmsBlackOpacity->value()),
                                        }));
    }

    return true;
}
