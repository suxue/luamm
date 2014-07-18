#include "luamm.hpp"
#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <utility>

using namespace boost::uuids;
using namespace luamm;

static std::string uuid_registry_key;


template<>
struct VarProxy<uuid> : VarBase {
    bool push(const uuid& u) {
        State st(state);
        UserData ud = st.newUserData<uuid>(std::move(u));
        ud.set(uuid_registry_key);
        ud.index = 0;
        return true;
    }
};

LUAMM_MODULE(uuid, L)
{
    State state(L);

    // uuid
    auto uuid_mod = std::move(
        state.class_<uuid>("uuid")
            .def("size", &uuid::size)
            .def("__tostring", [](UserData&& self) {
                return to_string(self.to<uuid>());
            })
    );
    uuid_registry_key = uuid_mod.uuid;

    auto c = state.class_<basic_random_generator<boost::mt19937>>("random_generator");
    c.def("__call", &basic_random_generator<boost::mt19937>::operator());
    c.init();
    Table mod = std::move(c);
    LUAMM_MODULE_RETURN(state, mod);
}
