#pragma once

#include <flecs.h>

// Forward declaration for the Skia Canvas object.
class SkCanvas;

/**
 * @brief The interface that all C++ scripts must implement.
 *
 * This acts as a contract between the main application and the dynamically
 * loaded script libraries. The engine will call these methods on the script
 * instance.
 */
struct IScript {
  virtual ~IScript() = default;

  /**
   * @brief Called once when the script is first attached to an entity.
   * @param entity The entity this script instance is attached to.
   * @param world The ECS world.
   */
  virtual void on_start(flecs::entity entity, flecs::world &world) = 0;

  /**
   * @brief Called every frame before rendering.
   * @param entity The entity this script instance is attached to.
   * @param world The ECS world.
   * @param dt The time elapsed since the last frame (delta time).
   * @param total_time The total elapsed time since the application started.
   */
  virtual void on_update(flecs::entity entity, flecs::world &world, float dt,
                         float total_time) = 0;

  /**
   * @brief Called during the rendering phase for this entity.
   * @param entity The entity this script instance is attached to.
   * @param world The ECS world.
   * @param canvas A pointer to the Skia canvas to draw on.
   */
  virtual void on_draw(flecs::entity entity, flecs::world &world,
                       SkCanvas *canvas) = 0;
};

// --- Factory Functions ---
// The engine will look for these C-style functions in the compiled shared
// library to create and destroy script instances. Using extern "C" prevents C++
// name mangling, ensuring the engine can find them by name.

extern "C" {
/**
 * @brief Factory function to create an instance of the script.
 * @return A pointer to a new instance of the IScript implementation.
 */
IScript *create_script();

/**
 * @brief Destroys a script instance created by create_script().
 * @param script A pointer to the script instance to be destroyed.
 */
void destroy_script(IScript *script);
}
