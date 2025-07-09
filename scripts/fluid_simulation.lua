--
-- A simple 2D particle-based fluid dynamics simulation.
--

-- Simulation constants
local NUM_PARTICLES = 150
local GRAVITY = 980.0
local PARTICLE_RADIUS = 5
local SMOOTHING_RADIUS = 25  -- The distance particles affect each other
local PRESSURE_MULTIPLIER = 500.0
local VISCOSITY_MULTIPLIER = 2.0
local WALL_DAMPING = 0.6

-- Simulation container dimensions
local bounds = { width = 400, height = 400 }

-- The table to hold all our particles
local particles = {}

-- Helper to create a new particle
function new_particle(x, y)
    return {
        x = x, y = y,           -- Current position
        ox = x, oy = y,         -- Old position (for Verlet integration)
        ax = 0, ay = 0            -- Acceleration
    }
end

-- Applies forces, integrates, and handles collisions
function update_physics(dt)
    for _, p in ipairs(particles) do
        -- 1. Apply forces
        -- Reset acceleration and apply gravity
        p.ax, p.ay = 0, GRAVITY

        -- Calculate forces from other particles
        for _, other in ipairs(particles) do
            if p ~= other then
                local dx = other.x - p.x
                local dy = other.y - p.y
                local dist_sq = dx * dx + dy * dy

                if dist_sq < SMOOTHING_RADIUS * SMOOTHING_RADIUS and dist_sq > 0 then
                    local dist = math.sqrt(dist_sq)
                    local normalized_dist = dist / SMOOTHING_RADIUS

                    -- Pressure (repulsion)
                    local pressure_force = PRESSURE_MULTIPLIER * (1.0 - normalized_dist)
                    p.ax = p.ax - dx / dist * pressure_force
                    p.ay = p.ay - dy / dist * pressure_force

                    -- Viscosity (cohesion)
                    local viscosity_force = VISCOSITY_MULTIPLIER * (1.0 - normalized_dist)
                    local rel_vx = (other.x - other.ox) - (p.x - p.ox)
                    local rel_vy = (other.y - other.oy) - (p.y - p.oy)
                    p.ax = p.ax + rel_vx * viscosity_force
                    p.ay = p.ay + rel_vy * viscosity_force
                end
            end
        end
    end

    -- 2. Integrate and handle collisions
    for _, p in ipairs(particles) do
        -- Verlet integration
        local vx = p.x - p.ox
        local vy = p.y - p.oy
        p.ox, p.oy = p.x, p.y
        p.x = p.x + vx + p.ax * dt * dt
        p.y = p.y + vy + p.ay * dt * dt

        -- Boundary collision
        if p.x < PARTICLE_RADIUS then
            p.x = PARTICLE_RADIUS
            p.ox = p.x + vx * WALL_DAMPING
        elseif p.x > bounds.width - PARTICLE_RADIUS then
            p.x = bounds.width - PARTICLE_RADIUS
            p.ox = p.x + vx * WALL_DAMPING
        end
        if p.y < PARTICLE_RADIUS then
            p.y = PARTICLE_RADIUS
            p.oy = p.y + vy * WALL_DAMPING
        elseif p.y > bounds.height - PARTICLE_RADIUS then
            p.y = bounds.height - PARTICLE_RADIUS
            p.oy = p.y + vy * WALL_DAMPING
        end
    end
end

function on_start(entity_id, registry)
    print("Fluid simulation script started for entity " .. tostring(entity_id))
    particles = {}
    -- Create a grid of particles
    local cols = 20
    local rows = math.floor(NUM_PARTICLES / cols)
    for i = 0, NUM_PARTICLES - 1 do
        local x = (i % cols) * 10 + bounds.width / 4
        local y = math.floor(i / cols) * 10 + bounds.height / 4
        particles[#particles + 1] = new_particle(x, y)
    end
end

function on_update(entity_id, registry, dt, currentTime)
    -- To keep the simulation stable, we run the physics update in fixed sub-steps
    local substeps = 2
    local substep_dt = dt / substeps
    for i = 1, substeps do
        update_physics(substep_dt)
    end
end

function on_draw(entity, registry, canvas)
    local paint = Paint.new()
    paint:setAntiAlias(true)
    paint:setColor(Color(128, 0, 150, 255)) -- Blueish-purple, semi-transparent
    paint:setStyle(true, false) -- Filled circles

    canvas:save()
    -- This script draws in its own container, relative to the entity's position.
    -- The renderer should have already translated the canvas to this entity's transform.

    -- Draw the particles
    for _, p in ipairs(particles) do
        canvas:drawCircle(p.x, p.y, PARTICLE_RADIUS * 2, paint)
    end

    -- Draw the container bounds for visualization
    paint:setStyle(false, true)
    paint:setStrokeWidth(2)
    paint:setColor(Color(255, 100, 100, 100))
    canvas:drawRect(0, 0, bounds.width, bounds.height, paint)

    canvas:restore()
end

function on_destroy(entity_id, registry)
    print("Fluid simulation script destroyed for entity " .. tostring(entity_id))
    particles = {}
end
