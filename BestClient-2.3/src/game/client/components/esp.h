// (c) Kinetix. ESP component — draws lines from point A to all players matching
// the configured filters. Ported to BestClient.
//
// Point A (the origin of all ESP lines) depends on Mode:
//   Active player     → interpolated render position of the active dummy.
//   Screen coordinates→ a fixed screen pixel, converted to world space. Useful
//                       during spectate or when the camera isn't on the dummy.
//
// Target players are filtered by team/friend/dummy/freeze. The active dummy
// itself is never a target (no self-line).
//
// Lines use the shared Line rendering settings (color/opacity/size).

#ifndef GAME_CLIENT_COMPONENTS_ESP_H
#define GAME_CLIENT_COMPONENTS_ESP_H

#include <base/color.h>
#include <base/vmath.h>
#include <game/client/component.h>

class CEsp : public CComponent
{
public:
	CEsp() = default;
	~CEsp() override = default;

	int Sizeof() const override { return sizeof(*this); }
	void OnReset() override;
	void OnRender() override;

private:
	// Same filter logic as the aimbot (team/friend/dummy/freeze).
	// Returns true if ClientId should be an ESP target.
	bool PassesFilters(int ClientId, int LocalId) const;

	// Style-aware line emission (Line/Arrow/Dotted/DottedArrow).
	void EmitLine(const vec2 &p0, const vec2 &p1) const;

	// Draw a single solid segment (immediate mode, not deferred).
	void DrawSegment(const vec2 &p0, const vec2 &p1, ColorRGBA col, float halfWidth) const;

	// Draw arrowhead at p1 (2 short lines forming a V pointing toward p1).
	void DrawArrowhead(const vec2 &p0, const vec2 &p1, ColorRGBA col, float halfWidth) const;
};

#endif // GAME_CLIENT_COMPONENTS_ESP_H