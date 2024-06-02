
#define DEFAULT_CTOR(Class) \
	Class() = default;      \
	~Class() = default;

//

#define DEFAULT_CP_CTOR(Class)   \
	Class(Class &rhs) = default; \
	Class &operator=(Class &rhs) = default;

#define DEFAULT_MV_CTOR(Class)    \
	Class(Class &&rhs) = default; \
	Class &operator=(Class &&rhs) = default;

//

#define DELETE_CP_CTOR(Class)   \
	Class(Class &rhs) = delete; \
	Class &operator=(Class &rhs) = delete;

#define DELETE_MV_CTOR(Class)    \
	Class(Class &&rhs) = delete; \
	Class &operator=(Class &&rhs) = delete;
