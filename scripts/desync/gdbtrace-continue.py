# Keeps the inferior running until it exits; batch mode would otherwise detach on the first error.
import gdb

errors = 0
command = "signal SIGCONT"
while gdb.selected_inferior().pid:
    try:
        gdb.execute(command)
        errors = 0
    except gdb.error as e:
        if "No such process" in str(e):
            break
        errors += 1
        print(f"gdbtrace: {e}", flush=True)
        if errors >= 20:
            raise
    command = "continue"
