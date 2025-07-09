--
-- A Multi-Layer Perceptron (MLP) simulation for generative art.
-- This script visualizes the output of a neural network by mapping
-- 2D coordinates to RGB colors. The network's weights are animated
-- over time to create a dynamic, evolving pattern.
--

-- Network configuration
local network = {}
-- Architecture: 2 inputs (x,y), two hidden layers with 8 neurons each, and 3 outputs (r,g,b)
local layer_config = {2, 8, 8, 3}

-- The activation function for the neurons. Tanh is a good choice for smooth visuals.
function tanh(x)
    return math.tanh(x)
end

-- Creates the network data structure and initializes it with random weights and biases.
function initialize_network()
    network = {}
    -- Iterate through each layer definition to create the layers
    for i = 1, #layer_config - 1 do
        local layer = {}
        local num_inputs = layer_config[i]
        local num_outputs = layer_config[i+1]
        -- Create each neuron in the current layer
        for j = 1, num_outputs do
            local neuron = {weights = {}, bias = math.random() * 2 - 1}
            -- For each neuron, create its incoming weights
            for k = 1, num_inputs do
                neuron.weights[#neuron.weights + 1] = math.random() * 2 - 1
            end
            layer[#layer + 1] = neuron
        end
        network[#network + 1] = layer
    end
end

-- Processes a set of inputs through the network to produce an output.
function feedforward(inputs)
    local current_inputs = inputs
    for _, layer in ipairs(network) do
        local new_outputs = {}
        for _, neuron in ipairs(layer) do
            -- Calculate the weighted sum of inputs plus the neuron's bias
            local activation = neuron.bias
            for k, weight in ipairs(neuron.weights) do
                activation = activation + weight * current_inputs[k]
            end
            -- Apply the activation function
            new_outputs[#new_outputs + 1] = tanh(activation)
        end
        current_inputs = new_outputs
    end
    return current_inputs
end

function on_start(entity_id, registry)
    print("MLP simulation script started for entity " .. tostring(entity_id))
    -- Seed the random number generator. If 'os' is not available, the pattern will be the same each run.
    if os and os.time then
        math.randomseed(os.time())
    end
    initialize_network()
end

function on_update(entity_id, registry, dt, currentTime)
    -- Animate the weights and biases slowly over time using sine waves for smooth transitions
    for i, layer in ipairs(network) do
        for j, neuron in ipairs(layer) do
            neuron.bias = neuron.bias + math.sin(currentTime * 0.2 + i*5 + j) * dt * 0.2
            for k, weight in ipairs(neuron.weights) do
                neuron.weights[k] = weight + math.cos(currentTime * 0.2 + i*10 + j + k) * dt * 0.2
            end
        end
    end
end

function on_draw(entity, registry, canvas)
    local paint = Paint.new()
    paint:setStyle(true, false) -- We will be drawing filled rectangles

    -- We draw a grid of cells. A higher resolution is more detailed but slower.
    local resolution = 30
    local bounds = {width = 500, height = 500}
    local cell_w = bounds.width / resolution
    local cell_h = bounds.height / resolution

    -- Iterate over every cell in our grid
    for x = 0, resolution - 1 do
        for y = 0, resolution - 1 do
            -- Normalize the cell's coordinates to the range [-1, 1].
            -- This is the standard input range for neural networks.
            local norm_x = (x / (resolution - 1)) * 2 - 1
            local norm_y = (y / (resolution - 1)) * 2 - 1

            -- Feed the coordinates into the network to get an output vector.
            local outputs = feedforward({norm_x, norm_y})

            -- Map the network's output from [-1, 1] to an RGB color [0, 255] and ensure they are integers.
            local r = math.floor((outputs[1] + 1) / 2 * 255)
            local g = math.floor((outputs[2] + 1) / 2 * 255)
            local b = math.floor((outputs[3] + 1) / 2 * 255)

            paint:setColor(Color(255, r, g, b))
            canvas:drawRect(x * cell_w, y * cell_h, cell_w, cell_h, paint)
        end
    end
end

function on_destroy(entity_id, registry)
    print("MLP simulation script destroyed for entity " .. tostring(entity_id))
end
