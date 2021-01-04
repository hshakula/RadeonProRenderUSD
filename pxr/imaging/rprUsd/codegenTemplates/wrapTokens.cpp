/************************************************************************
Copyright 2020 Advanced Micro Devices, Inc
Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at
    http://www.apache.org/licenses/LICENSE-2.0
Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
************************************************************************/

// GENERATED FILE.  DO NOT EDIT.
#include <hboost/python/class.hpp>
#include "{{ libraryPath }}/tokens.h"

{% if useExportAPI %}
{{ namespaceUsing }}

{% endif %}
namespace {

// Helper to return a static token as a string.  We wrap tokens as Python
// strings and for some reason simply wrapping the token using def_readonly
// bypasses to-Python conversion, leading to the error that there's no
// Python type for the C++ TfToken type.  So we wrap this functor instead.
class _WrapStaticToken {
public:
    _WrapStaticToken(const TfToken* token) : _token(token) { }

    std::string operator()() const
    {
        return _token->GetString();
    }

private:
    const TfToken* _token;
};

template <typename T>
void
_AddToken(T& cls, const char* name, const TfToken& token)
{
    cls.add_static_property(name,
                            hboost::python::make_function(
                                _WrapStaticToken(&token),
                                hboost::python::return_value_policy<
                                    hboost::python::return_by_value>(),
                                hboost::mpl::vector1<std::string>()));
}

} // anonymous

void wrap{{ tokensPrefix }}Tokens()
{
    hboost::python::class_<{{ tokensPrefix }}TokensType, hboost::noncopyable>
        cls("Tokens", hboost::python::no_init);
{% for token in tokens %}
    _AddToken(cls, "{{ token.id }}", {{ tokensPrefix }}Tokens->{{ token.id }});
{% endfor %}
}
