--
-- A procedural creature that responds to its transform component.
-- It jiggles based on velocity and its eyes look in the direction of movement.
--

-- Helper function to draw an oval by creating a procedural path.
function draw_oval(canvas, paint, cx, cy, rx, ry)
    local path = Path.new()
    local segments = 36
    path:moveTo(cx + rx, cy)
    for i = 1, segments do
        local angle = (i / segments) * 2 * math.pi
        local px = cx + math.cos(angle) * rx
        local py = cy + math.sin(angle) * ry
        path:lineTo(px, py)
    end
    path:close()
    canvas:drawPath(path, paint)
end

-- Animation & State parameters
local body_size_x = 50
local body_size_y = 80
local leg_count = 6
local leg_length = 80
local leg_segments = 10
local wiggle_speed = 6
local wiggle_amplitude = 15
local current_time = 0

-- State for tracking movement
local last_x = nil
local last_y = nil
local jiggle_factor = 2
local look_dx = 0
local look_dy = 0

function on_start(entity_id, registry)
    print("Reactive creature script started for entity " .. tostring(entity_id))
    -- Reset state on start
    last_x = nil
    last_y = nil
end

function on_update(entity_id, registry, dt, time)
    current_time = time

    -- NOTE: We are assuming the API to get a transform component is 'Transform.get(entity_id)'.
    -- This may need to be adjusted if the actual C++/Lua binding is different.
    local transform = registry:get_transform(entity_id)

    if transform then
        if last_x == nil then -- First frame, initialize position
            last_x = transform.x
            last_y = transform.y
        end

        -- Calculate velocity and speed. Avoid division by zero if dt is 0.
        local vx = (dt > 0) and (transform.x - last_x) / dt or 0
        local vy = (dt > 0) and (transform.y - last_y) / dt or 0
        local speed = math.sqrt(vx*vx + vy*vy)

        -- Update jiggle based on speed (with some smoothing/damping)
        local target_jiggle = math.min(speed / 40, 15) -- Clamp max jiggle
        jiggle_factor = jiggle_factor + (target_jiggle - jiggle_factor) * 0.1

        -- Update look direction based on velocity (with smoothing)
        if speed > 1 then
            local target_look_x = (vx / speed) * 6
            local target_look_y = (vy / speed) * 4
            look_dx = look_dx + (target_look_x - look_dx) * 0.1
            look_dy = look_dy + (target_look_y - look_dy) * 0.1
        else
            -- Smoothly return to center when not moving
            look_dx = look_dx + (0 - look_dx) * 0.1
            look_dy = look_dy + (0 - look_dy) * 0.1
        end

        -- Store position for the next frame
        last_x = transform.x
        last_y = transform.y
    end

    -- Body pulsation, now influenced by jiggle
    body_size_x = 50 + math.sin(current_time * 2) * 5 + jiggle_factor
    body_size_y = 80 + math.cos(current_time * 2) * 8 - jiggle_factor
end

function on_draw(entity, registry, canvas)
    local paint = Paint.new()
    paint:setAntiAlias(true)

    canvas:save()
    -- The creature is now drawn at its local origin (0,0).
    -- The renderer is expected to have translated the canvas to the entity's world position.

    -- === Draw Legs (behind the body) ===
    paint:setColor(Color(255, 80, 20, 40))
    paint:setStyle(false, true)
    paint:setStrokeWidth(8)
    local current_wiggle_amplitude = wiggle_amplitude + jiggle_factor

    for i = 1, leg_count do
        canvas:save()
        canvas:rotate(360 / leg_count * i)
        local path = Path.new()
        path:moveTo(0, body_size_y * 0.4)
        for j = 1, leg_segments do
            local x = math.sin(current_time * wiggle_speed + i) * current_wiggle_amplitude * (j / leg_segments)
            local y = body_size_y * 0.4 + (leg_length / leg_segments) * j
            path:lineTo(x, y)
        end
        canvas:drawPath(path, paint)
        canvas:restore()
    end

    -- === Draw Body ===
    paint:setColor(Color(255, 100, 40, 80))
    paint:setStyle(true, false)
    draw_oval(canvas, paint, 0, 0, body_size_x / 2, body_size_y / 2)
    paint:setColor(Color(255, 120, 60, 120))
    paint:setStyle(false, true)
    paint:setStrokeWidth(3)
    draw_oval(canvas, paint, 0, 0, body_size_x / 2, body_size_y / 2)

    -- === Draw Eyes ===
    paint:setColor(Color(255, 255, 255, 255))
    paint:setStyle(true, false)
    local eye_y = -body_size_y * 0.2
    local eye_x_offset = body_size_x * 0.25
    local eye_height = 18
    if (current_time % 4 < 0.15) then eye_height = 2 end
    draw_oval(canvas, paint, -eye_x_offset, eye_y, 12, eye_height / 2)
    draw_oval(canvas, paint, eye_x_offset, eye_y, 12, eye_height / 2)

    -- Pupils now use the smoothed look direction
    paint:setColor(Color(255, 0, 0, 0))
    canvas:drawCircle(-eye_x_offset + look_dx, eye_y + look_dy, 5, paint)
    canvas:drawCircle(eye_x_offset + look_dx, eye_y + look_dy, 5, paint)

    canvas:restore()
end

function on_destroy(entity_id, registry)
    print("Reactive creature script destroyed for entity " .. tostring(entity_id))
end
