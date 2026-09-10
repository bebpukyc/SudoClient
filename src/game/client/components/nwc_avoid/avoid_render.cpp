#include "avoid.h"

#include "../../gameclient.h"

#include <base/color.h>
#include <base/math.h>

#include <engine/graphics.h>

void CNwcAvoid::RenderDebug() const
{
	if(!g_Config.m_NwAvoidDebug || !g_Config.m_NwAvoidEnabled)
		return;

	const int DummyIndex = g_Config.m_ClDummy;
	if(!in_range(DummyIndex, NUM_DUMMIES - 1))
		return;

	const SNwcAvoidRuntime &Runtime = m_aRuntime[DummyIndex];
	if(Runtime.m_vDebugTrace.empty() && !Runtime.m_HasDebugHookPoint)
		return;

	float WorldW = 0.0f;
	float WorldH = 0.0f;
	Graphics()->CalcScreenParams(Graphics()->ScreenAspect(), GameClient()->m_Camera.m_Zoom, &WorldW, &WorldH);
	const float X0 = GameClient()->m_Camera.m_Center.x - WorldW / 2.0f;
	const float Y0 = GameClient()->m_Camera.m_Center.y - WorldH / 2.0f;
	Graphics()->MapScreen(X0, Y0, X0 + WorldW, Y0 + WorldH);

	Graphics()->TextureClear();
	Graphics()->LinesBegin();
	for(size_t i = 1; i < Runtime.m_vDebugTrace.size(); ++i)
	{
		const SNwcAvoidTracePoint &From = Runtime.m_vDebugTrace[i - 1];
		const SNwcAvoidTracePoint &To = Runtime.m_vDebugTrace[i];
		const ColorRGBA Color = To.m_Danger ? ColorRGBA(1.0f, 0.22f, 0.20f, 0.95f) : ColorRGBA(0.22f, 0.95f, 0.72f, 0.90f);
		Graphics()->SetColor(Color.r, Color.g, Color.b, Color.a);
		const IGraphics::CLineItem Line(From.m_Pos.x, From.m_Pos.y, To.m_Pos.x, To.m_Pos.y);
		Graphics()->LinesDraw(&Line, 1);
	}
	if(Runtime.m_HasDebugHookPoint && !Runtime.m_vDebugTrace.empty())
	{
		Graphics()->SetColor(0.34f, 0.84f, 1.0f, 0.85f);
		const vec2 Start = Runtime.m_vDebugTrace.front().m_Pos;
		const IGraphics::CLineItem HookLine(Start.x, Start.y, Runtime.m_DebugHookPoint.x, Runtime.m_DebugHookPoint.y);
		Graphics()->LinesDraw(&HookLine, 1);
	}
	Graphics()->LinesEnd();

	if(Runtime.m_HasDebugHookPoint)
	{
		Graphics()->TextureClear();
		Graphics()->QuadsBegin();
		Graphics()->SetColor(0.34f, 0.84f, 1.0f, 0.22f);
		Graphics()->DrawCircle(Runtime.m_DebugHookPoint.x, Runtime.m_DebugHookPoint.y, 12.0f, 20);
		Graphics()->QuadsEnd();
	}
}

void CNwcAvoid::OnRender()
{
	RenderDebug();
}

