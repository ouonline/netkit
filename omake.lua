project = CreateProject()

dep = project:CreateDependency()
dep:AddSourceFiles("*.cpp")
dep:AddFlags("-Wall", "-Werror", "-Wextra", "-fPIC")
dep:AddStaticLibrary("../threadkit", "threadkit_static")
dep:AddStaticLibrary("../logger", "logger_static")

a = project:CreateStaticLibrary("netkit_static")
a:AddDependencies(dep)

so = project:CreateSharedLibrary("netkit_shared")
so:AddDependencies(dep)

return project
