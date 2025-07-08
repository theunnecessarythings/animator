local rotation_angle = 0
local circle_radius = 50

function on_start(entity_id, registry)
    print("Sample draw script started for entity " .. tostring(entity_id))
end

function on_update(entity_id, registry, dt, currentTime)
    rotation_angle = rotation_angle + 30 * dt
    circle_radius = 50 + math.sin(currentTime * 5) * 20
end

function on_draw(entity, registry, canvas)
    local paint = Paint.new()
    paint:setAntiAlias(true)

    -- 1. Draw a rotating square (stroked)
    paint:setColor(Color(255, 255, 0, 0))
    paint:setStyle(false, true)
    paint:setStrokeWidth(3)

    canvas:save()
    canvas:translate(100, 100)
    canvas:rotate(rotation_angle)
    canvas:drawRect(-50, -50, 100, 100, paint)
    canvas:restore()

    -- 2. Draw a pulsating circle (filled)
    paint:setColor(Color(math.floor(rotation_angle) % 255, 0, 0, 255)) 
    paint:setStyle(true, false) -- Set style to fill (fill=true, stroke=false)

    canvas:save()
    canvas:translate(300, 100)
    canvas:drawCircle(0, 0, circle_radius, paint)
    canvas:restore()

    -- 3. Draw a custom path (filled and stroked triangle)
    local path = Path.new()
    path:moveTo(0, 0)
    path:lineTo(100, 0)
    path:lineTo(50, 86.6)
    path:close()
    
    -- Now, configure the paint for the path
    paint:setColor(Color(255, 0, 255, 0))
    paint:setStyle(true, true) -- Set style to fill AND stroke
    paint:setStrokeWidth(5)
    
    canvas:save()
    canvas:translate(200, 300)
    canvas:drawPath(path, paint) -- Draw the path object we created
    canvas:restore()
end

function on_destroy(entity_id, registry)
    print("Sample draw script destroyed for entity " .. tostring(entity_id))
end
