--
-- A script to animate an entity along a procedurally generated Bezier spline.
-- It demonstrates path following, progressive path drawing, and camera control.
--

-- The generated path segments
local path_segments = {}
-- The currently drawn path, which grows over time
local drawn_path = nil
-- Total number of segments in the spline
local num_segments = 15
-- The duration for one full loop of the animation
local animation_duration = 20.0 -- in seconds

-- Helper function for linear interpolation between two points
function lerp(pA, pB, t)
    return {x = (1-t)*pA.x + t*pB.x, y = (1-t)*pA.y + t*pB.y}
end

-- Helper function to get the control points for a sub-section of a Bezier curve
function get_bezier_sub_segment(p0, p1, p2, p3, t)
    local p10 = lerp(p0, p1, t)
    local p11 = lerp(p1, p2, t)
    local p12 = lerp(p2, p3, t)
    local p20 = lerp(p10, p11, t)
    local p21 = lerp(p11, p12, t)
    local p30 = lerp(p20, p21, t)
    return p0, p10, p20, p30
end

-- Helper function to evaluate a cubic Bezier curve at time t
function evaluate_bezier(p0, p1, p2, p3, t)
    local one_minus_t = 1.0 - t
    local one_minus_t_sq = one_minus_t * one_minus_t
    local t_sq = t * t

    local x = one_minus_t_sq * one_minus_t * p0.x +
              3 * one_minus_t_sq * t * p1.x +
              3 * one_minus_t * t_sq * p2.x +
              t_sq * t * p3.x

    local y = one_minus_t_sq * one_minus_t * p0.y +
              3 * one_minus_t_sq * t * p1.y +
              3 * one_minus_t * t_sq * p2.y +
              t_sq * t * p3.y

    return {x = x, y = y}
end

-- Helper function to calculate the tangent of a cubic Bezier curve at time t
function evaluate_bezier_tangent(p0, p1, p2, p3, t)
    local one_minus_t = 1.0 - t
    local t_sq = t * t

    local tx = 3 * one_minus_t * one_minus_t * (p1.x - p0.x) +
               6 * one_minus_t * t * (p2.x - p1.x) +
               3 * t_sq * (p3.x - p2.x)

    local ty = 3 * one_minus_t * one_minus_t * (p1.y - p0.y) +
               6 * one_minus_t * t * (p2.y - p1.y) +
               3 * t_sq * (p3.y - p2.y)

    return {x = tx, y = ty}
end


function on_start(entity_id, registry)
    print("Path animation script started for entity " .. tostring(entity_id))

    if os and os.time then
        math.randomseed(os.time())
    end

    path_segments = {}
    drawn_path = Path.new()

    -- Generate the first point randomly
    local p0 = { x = math.random(100, 400), y = math.random(100, 400) }
    drawn_path:moveTo(p0.x, p0.y)

    local p1 = { x = p0.x + math.random(-150, 150), y = p0.y + math.random(-150, 150) }
    local p2 = { x = p0.x + math.random(-50, 250), y = p0.y + math.random(-50, 250) }
    local p3 = { x = p2.x + math.random(-150, 150), y = p2.y + math.random(-150, 150) }
    path_segments[#path_segments + 1] = {p0=p0, p1=p1, p2=p2, p3=p3}

    -- Generate subsequent segments to be smooth
    for i = 2, num_segments do
        local prev_p3 = p3
        local prev_p2 = p2

        p0 = prev_p3
        -- Reflected control point for C1 continuity
        p1 = { x = 2 * p0.x - prev_p2.x, y = 2 * p0.y - prev_p2.y }

        -- New random control points
        p2 = { x = p0.x + math.random(-100, 100), y = p0.y + math.random(-100, 100) }
        p3 = { x = p2.x + math.random(-150, 150), y = p2.y + math.random(-150, 150) }
        path_segments[#path_segments + 1] = {p0=p0, p1=p1, p2=p2, p3=p3}
    end
end

function on_update(entity_id, registry, dt, total_time)
    local transform = registry:get_transform(entity_id)
    if not transform then return end

    -- Calculate the current progress through the animation loop
    local progress = (total_time / animation_duration) % 1.0

    -- Determine which segment and what 't' value we are at
    local total_segments = #path_segments
    local current_segment_progress = progress * total_segments
    local segment_index = math.floor(current_segment_progress)
    local t = current_segment_progress - segment_index

    local segment = path_segments[segment_index + 1]
    if not segment then return end

    -- Calculate the position and tangent on the curve
    local pos = evaluate_bezier(segment.p0, segment.p1, segment.p2, segment.p3, t)
    local tan = evaluate_bezier_tangent(segment.p0, segment.p1, segment.p2, segment.p3, t)

    -- Update the entity's transform
    transform.x = pos.x
    transform.y = pos.y
    transform.rotation = math.atan2(tan.y, tan.x)

    -- Center the camera on the entity
    local center_x, center_y = Camera.get_center()
    Camera.pan(center_x - pos.x, center_y - pos.y)

    -- Update the drawn path for progressive rendering
    drawn_path = Path.new()
    if #path_segments > 0 then
        drawn_path:moveTo(path_segments[1].p0.x, path_segments[1].p0.y)
    end

    -- Draw all completed segments fully
    for i = 1, segment_index do
        local s = path_segments[i]
        drawn_path:cubicTo(s.p1.x, s.p1.y, s.p2.x, s.p2.y, s.p3.x, s.p3.y)
    end

    -- Draw the current segment partially
    local s = path_segments[segment_index + 1]
    if s and t > 0 then
        local sp0, sp1, sp2, sp3 = get_bezier_sub_segment(s.p0, s.p1, s.p2, s.p3, t)
        drawn_path:cubicTo(sp1.x, sp1.y, sp2.x, sp2.y, sp3.x, sp3.y)
    end
end

function on_draw(entity, registry, canvas)
    local paint = Paint.new()
    paint:setAntiAlias(true)
    paint:setColor(Color(255, 255, 255, 255)) -- White

    -- The canvas is already translated to the entity's position and rotation.
    -- To draw the full path in world space, we must undo this transformation.
    local transform = registry:get_transform(entity)
    if not transform then return end

    canvas:save()
    -- Undo the entity's transformation
    canvas:rotate(-transform.rotation * 180 / math.pi)
    canvas:translate(-transform.x, -transform.y)

    -- Draw the progressively drawn spline
    -- Draw the progressively drawn spline with glow
    if drawn_path then
        -- Create a blur mask filter for the glow effect
        local blur_radius = 8.0 -- Adjust as needed
        local blur_mask_filter = CreateBlurMaskFilter(blur_radius, BlurStyle.Normal)

        -- Draw the glow layer
        paint:setColor(Color(255, 255, 255, 200)) -- White with some transparency
        paint:setStrokeWidth(8) -- Wider stroke for the glow
        paint:setStrokeCap(PaintCap.Round)
        paint:setMaskFilter(blur_mask_filter)
        canvas:drawPath(drawn_path, paint)

        -- Draw the main path on top
        paint:setColor(Color(255, 255, 255, 255)) -- Opaque white
        paint:setStrokeWidth(4)
        paint:setStrokeCap(PaintCap.Round)
        paint:setMaskFilter(nil) -- Remove mask filter for the main path
        canvas:drawPath(drawn_path, paint)
    end

    canvas:restore()

    -- Draw the highlight circle at the entity's local origin (0,0) as an annulus.
    -- The canvas is already correctly transformed for this.
    paint:setStyle(false, true) -- Stroke (for annulus)
    paint:setStrokeWidth(3) -- Annulus thickness
    paint:setColor(Color(255, 255, 255, 255)) -- White
    paint:setMaskFilter(nil) -- Ensure no mask filter for the circle
    canvas:drawCircle(0, 0, 8, paint)
end

function on_destroy(entity_id, registry)
    print("Path animation script destroyed for entity " .. tostring(entity_id))
    path_segments = {}
    drawn_path = nil
end
