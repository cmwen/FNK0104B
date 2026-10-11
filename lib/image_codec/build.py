Import("env")
env.Append(CPPPATH=[env.Dir("src/vendor/libwebp").abspath])
