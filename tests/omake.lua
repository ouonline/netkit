project = CreateProject()

dep = project:CreateDependency()
dep:AddSourceFiles("*.cpp")
dep:AddFlags("-Wall", "-Werror", "-Wextra")
dep:AddStaticLibrary("..", "netkit_static")

target = project:CreateBinary("echo_server_and_client")
target:AddDependencies(dep)

return project
