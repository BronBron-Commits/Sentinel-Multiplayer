#ifndef RUNMODE_HPP
#define RUNMODE_HPP

// Shared run mode for desktop/VR

enum class RunMode {
    Desktop,
    VR
};

extern RunMode g_run_mode;

#endif // RUNMODE_HPP
