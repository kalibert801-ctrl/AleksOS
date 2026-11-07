# Эмулятор — самый горячий код: собираем src/emulator с -O2 вместо общего -Os.
# -O2 добавляется в конец CCFLAGS, а побеждает последний флаг -O в командной строке.
Import("env")


def emulator_o2(env, node):
    return env.Object(node, CCFLAGS=env["CCFLAGS"] + ["-O2"])


env.AddBuildMiddleware(emulator_o2, "*emulator*")
