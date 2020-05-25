project = CreateProject()

dep = project:CreateDependency()
    :AddSourceFiles("*.cpp")
    :AddFlags("-Wall", "-Werror", "-Wextra", "-fPIC")
    :AddStaticLibrary("../threadkit", "threadkit_static")
    :AddStaticLibrary("../logger", "logger_static")

project:CreateStaticLibrary("netkit_static"):AddDependencies(dep)

project:CreateSharedLibrary("netkit_shared"):AddDependencies(dep)

return project
