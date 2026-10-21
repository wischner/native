"""End the SSH debug session when its inferior exits, retaining live stops."""

import os
import threading

import lldb


def _wait_for_exit(debugger, process, listener):
    """Listen independently of LLDB's command and process-event consumers."""
    event = lldb.SBEvent()
    while process.IsValid():
        listener.WaitForEvent(1, event)
        if process.GetState() == lldb.eStateExited:
            print("Native: process exited; ending debugger session.", flush=True)
            # HandleCommand("quit") only returns a quit result to a Python
            # caller; it does not close the CLI driver's input loop. The
            # inferior has finished, so end this debugger process as well.
            status = process.GetExitStatus()
            os._exit(status if 0 <= status <= 255 else 1)
            return


def __lldb_init_module(debugger, internal_dict):
    """Subscribe at the entry stop before continuing the process."""
    listener = lldb.SBListener("native.process.exit")
    process = debugger.GetSelectedTarget().GetProcess()
    listener.StartListeningForEvents(
        process.GetBroadcaster(),
        lldb.SBProcess.eBroadcastBitStateChanged,
    )
    threading.Thread(
        target=_wait_for_exit, args=(debugger, process, listener), daemon=True
    ).start()
