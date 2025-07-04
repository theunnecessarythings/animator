-- scripts/bouncing_ball.lua

local initial_y
local velocity_y = 0
local gravity = 980 -- pixels/second^2
local bounce_factor = 0.8 -- How much velocity is retained after a bounce
local floor_y = 400 -- The 'ground' level

function on_start(entity_id, registry)
    print("Bouncing ball script started for entity " .. entity_id)
    local transform = registry:get_transform(entity_id)
    if transform then
        initial_y = transform.y
    end
end

function on_update(entity_id, registry, dt, currentTime)
    local transform = registry:get_transform(entity_id)
    if transform then
        -- Apply gravity
        velocity_y = velocity_y + gravity * dt
        transform.y = transform.y + velocity_y * dt

        -- Bounce off the floor
        if transform.y > floor_y then
            transform.y = floor_y
            velocity_y = -velocity_y * bounce_factor
        end

        -- Simple horizontal movement (optional)
        transform.x = transform.x + 50 * dt
        if transform.x > 800 then
            transform.x = 0
        end

        -- Example of using currentTime for stateless animation (e.g., pulsating color)
        local material = registry:get_material(entity_id)
        if material then
            local r = math.floor(math.sin(currentTime * 2) * 127 + 128)
            local g = math.floor(math.sin(currentTime * 2 + math.pi / 2) * 127 + 128)
            local b = math.floor(math.sin(currentTime * 2 + math.pi) * 127 + 128)
            material.color = (r << 24) + (g << 16) + (b << 8) + 0xFF -- RGBA
        else
            print("No material found for entity " .. entity_id)
        end
    end
end

function on_destroy(entity_id, registry)
    print("Bouncing ball script destroyed for entity " .. entity_id)
end
