// ==============================================================================
// CDMF (Component-based Dynamic Module Framework)
// All-in-one Convenience Library
// ==============================================================================

/**
 * @file cdmf.cpp
 * @brief All-in-one convenience library implementation
 *
 * This file serves as the implementation for the cdmf all-in-one library.
 * The library links together all CDMF component libraries:
 * - cdmf_foundation (BlockingQueue, ThreadPool)
 * - cdmf_platform (PlatformAbstraction, DynamicLoader)
 * - cdmf_core (Version, Properties, Event, EventDispatcher)
 * - cdmf_module (ModuleHandle, ModuleImpl, ModuleRegistry)
 * - cdmf_service (ServiceReference, ServiceRegistry, ServiceTracker)
 *
 * All functionality is provided through the linked component libraries.
 * This file exists primarily to create the library target.
 */

namespace cdmf {

// Version information for the unified library
const char* getCDMFVersion() {
    return "1.0.0";
}

const char* getCDMFBuildInfo() {
    return "CDMF Framework - All-in-one library";
}

} // namespace cdmf
