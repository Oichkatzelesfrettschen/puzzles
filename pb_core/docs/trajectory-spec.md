# pb_trajectory - Cleanroom Specification

## Purpose
Trajectory prediction system for shot simulation and aiming assistance.

## Algorithm 1: Line-Circle Intersection (Quadratic)

Given a line from point P0=(x0,y0) traveling in direction D=(dx,dy),
detect intersection with a circle at center C=(cx,cy) with radius R.

**Parametric line:** P(t) = P0 + t*D where t >= 0

**Circle equation:** |P - C|^2 = R^2

**Substitution yields quadratic:** a*t^2 + b*t + c = 0

Where:
- a = dx^2 + dy^2 (= 1 if direction normalized)
- b = 2 * (dx*(x0-cx) + dy*(y0-cy))
- c = (x0-cx)^2 + (y0-cy)^2 - R^2

**Solution:**
- discriminant = b^2 - 4*a*c
- If discriminant < 0: no intersection
- If discriminant >= 0: t = (-b +/- sqrt(discriminant)) / (2*a)
- Smallest positive t gives first intersection point

## Algorithm 2: Wall Bounce Reflection

When ball hits vertical wall, reflect horizontal velocity component:
- new_dx = -dx
- new_dy = dy (unchanged)

Geometrically: reflect velocity about the wall normal vector.

**General reflection formula:** v' = v - 2*(v.n)*n
where n is unit normal to surface.

For left/right walls: n = (1,0) or (-1,0)

## Algorithm 3: Trajectory Segment Chain

A trajectory consists of multiple segments:
1. Start segment from cannon position with initial velocity
2. For each segment:
   - Find closest collision (wall, ceiling, or bubble)
   - If wall: create reflected segment and continue
   - If ceiling/bubble: terminate trajectory
3. Record collision points for rendering aim guide

## Algorithm 4: Grid Snapping

Convert final collision point to grid cell:
1. Transform pixel coordinates to axial/offset coordinates
2. Find nearest cell center within valid range
3. If occupied, search neighboring cells for empty slot
4. Prefer cells adjacent to the hit bubble

## Constants

- Combined collision radius: 2 * bubble_radius * reduction_factor
- Reduction factor: ~0.925 (allows near-misses)
- Maximum bounces: configurable (typically 2-4)

## API Design

```c
// Trajectory segment
typedef struct pb_trajectory_segment {
    pb_point start;
    pb_point end;
    bool is_terminal;  // hit ceiling or bubble
} pb_trajectory_segment;

// Compute full trajectory
int pb_trajectory_compute(
    pb_point origin,
    pb_scalar angle,
    pb_scalar speed,
    const pb_board* board,
    pb_scalar radius,
    pb_scalar left_wall,
    pb_scalar right_wall,
    pb_scalar ceiling,
    int max_bounces,
    pb_trajectory_segment* segments,
    int max_segments
);

// Get final landing cell
pb_offset pb_trajectory_landing_cell(
    const pb_trajectory_segment* segments,
    int segment_count,
    const pb_board* board,
    pb_scalar radius
);
```
