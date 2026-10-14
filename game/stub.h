#pragma once

struct StubOptions
{
    bool          stub = false;   // --stub given
    bool          hidden = false;   // --hidden: off-screen tool window (no taskbar button)
    unsigned long parentPid = 0;       // --parent <pid>
};

StubOptions ParseStubCommandLine();
int         RunStub(const StubOptions& options);