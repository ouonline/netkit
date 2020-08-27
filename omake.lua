project = Project()

dep = project:CreateDependency()
    :AddSourceFiles("*.cpp")
    :AddFlags({"-Wall", "-Werror", "-Wextra", "-fPIC"})
    :AddStaticLibraries("../threadkit", "threadkit_static")
    :AddStaticLibraries("../logger", "logger_static")

project:CreateStaticLibrary("netkit_static"):AddDependencies(dep)
project:CreateSharedLibrary("netkit_shared"):AddDependencies(dep)

return project
