#include "utils.hpp"

/**
 * @brief Adding two integers
 *
 * @startuml add
 * !include _theme_light.puml
 * rectangle Adding as ad
 * rectangle First as f
 * rectangle Second as s
 * ad -> f
 * ad -> s
 * @enduml
 */
int add(int a, int b) { return a + b; }

int mul(int a, int b) { return a * b; }
