import os

import os

# Resolve paths relative to this script
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
FP_HPP = os.path.abspath(os.path.join(SCRIPT_DIR, "../Source/ForbocAI_SDK/Public/Core/functional_core.hpp"))
RTK_HPP = os.path.abspath(os.path.join(SCRIPT_DIR, "../Source/ForbocAI_SDK/Public/Core/rtk.hpp"))

with open(FP_HPP, "r") as f:
    fp_text = f.read()

# Fix fmap for vector (lines ~527)
fp_text = fp_text.replace("""template <typename T, typename Func>
auto fmap(const std::vector<T> &vec, Func f)
    -> std::vector<decltype(f(std::declval<const T &>()))> {
  typedef decltype(f(std::declval<const T &>())) U;
  std::vector<U> result;
  result.reserve(vec.size());
  for (const auto &item : vec) {
    result.push_back(f(item));
  }
  return result;
}""", """namespace detail {
template <typename T, typename U, typename Func>
void fmap_vec_recursive(typename std::vector<T>::const_iterator current, typename std::vector<T>::const_iterator end, Func f, std::vector<U>& out) {
  return current == end ? void() : (out.push_back(f(*current)), fmap_vec_recursive<T, U, Func>(current + 1, end, f, out));
}
} // namespace detail

template <typename T, typename Func>
auto fmap(const std::vector<T> &vec, Func f)
    -> std::vector<decltype(f(std::declval<const T &>()))> {
  typedef decltype(f(std::declval<const T &>())) U;
  std::vector<U> result;
  result.reserve(vec.size());
  detail::fmap_vec_recursive<T, U, Func>(vec.begin(), vec.end(), f, result);
  return result;
}""")

# Fix mbind and ebind
fp_text = fp_text.replace("""template <typename T, typename Func>
auto mbind(const Maybe<T> &m, Func f) -> decltype(f(m.value)) {
  if (!m.hasValue)
    return decltype(f(m.value)){false, {}};
  return f(m.value);
}""", """template <typename T, typename Func>
auto mbind(const Maybe<T> &m, Func f) -> decltype(f(m.value)) {
  return m.hasValue ? f(m.value) : decltype(f(m.value)){false, {}};
}""")

fp_text = fp_text.replace("""template <typename E, typename T, typename Func>
auto ebind(const Either<E, T> &e, Func f) -> decltype(f(e.right)) {
  if (e.isLeft)
    return decltype(f(e.right)){true, e.left, {}};
  return f(e.right);
}""", """template <typename E, typename T, typename Func>
auto ebind(const Either<E, T> &e, Func f) -> decltype(f(e.right)) {
  return e.isLeft ? decltype(f(e.right)){true, e.left, {}} : f(e.right);
}""")

# Fix fmap for Maybe
fp_text = fp_text.replace("""template <typename T, typename Func>
auto fmap(const Maybe<T> &m, Func f) -> Maybe<decltype(f(m.value))> {
  typedef decltype(f(m.value)) U;
  if (!m.hasValue)
    return Maybe<U>{false, U{}};
  return Maybe<U>{true, f(m.value)};
}""", """template <typename T, typename Func>
auto fmap(const Maybe<T> &m, Func f) -> Maybe<decltype(f(m.value))> {
  typedef decltype(f(m.value)) U;
  return m.hasValue ? Maybe<U>{true, f(m.value)} : Maybe<U>{false, U{}};
}""")

# Fix fmap for Either
fp_text = fp_text.replace("""template <typename E, typename T, typename Func>
auto fmap(const Either<E, T> &e, Func f) -> Either<E, decltype(f(e.right))> {
  typedef decltype(f(e.right)) U;
  if (e.isLeft)
    return Either<E, U>{true, e.left, U{}};
  return Either<E, U>{false, E{}, f(e.right)};
}""", """template <typename E, typename T, typename Func>
auto fmap(const Either<E, T> &e, Func f) -> Either<E, decltype(f(e.right))> {
  typedef decltype(f(e.right)) U;
  return e.isLeft ? Either<E, U>{true, e.left, U{}} : Either<E, U>{false, E{}, f(e.right)};
}""")

# Fix ematch and match (already using ternary in my previous look, let me double check. Oh wait, ematch and match had if statements!)
fp_text = fp_text.replace("""template <typename T, typename FJust, typename FNothing>
auto match(const Maybe<T> &m, FJust onJust, FNothing onNothing)
    -> decltype(onJust(m.value)) {
  if (m.hasValue)
    return onJust(m.value);
  return onNothing();
}""", """template <typename T, typename FJust, typename FNothing>
auto match(const Maybe<T> &m, FJust onJust, FNothing onNothing)
    -> decltype(onJust(m.value)) {
  return m.hasValue ? onJust(m.value) : onNothing();
}""")

fp_text = fp_text.replace("""template <typename E, typename T, typename FLeft, typename FRight>
auto ematch(const Either<E, T> &e, FLeft onLeft, FRight onRight)
    -> decltype(onRight(e.right)) {
  if (e.isLeft)
    return onLeft(e.left);
  return onRight(e.right);
}""", """template <typename E, typename T, typename FLeft, typename FRight>
auto ematch(const Either<E, T> &e, FLeft onLeft, FRight onRight)
    -> decltype(onRight(e.right)) {
  return e.isLeft ? onLeft(e.left) : onRight(e.right);
}""")

# Fix multi_match
fp_text = fp_text.replace("""template <typename T, typename R>
Maybe<R> multi_match(const T &value, const std::vector<MatchCase<T, R>> &cases) {
  for (size_t i = 0; i < cases.size(); ++i) {
    if (cases[i].predicate(value)) {
      return just(cases[i].handler(value));
    }
  }
  return nothing<R>();
}""", """namespace detail {
template <typename T, typename R>
Maybe<R> multi_match_recursive(const T &value, const std::vector<MatchCase<T, R>> &cases, size_t index) {
  return index == cases.size() ? nothing<R>() :
         (cases[index].predicate(value) ? just(cases[index].handler(value)) :
          multi_match_recursive(value, cases, index + 1));
}
} // namespace detail

template <typename T, typename R>
Maybe<R> multi_match(const T &value, const std::vector<MatchCase<T, R>> &cases) {
  return detail::multi_match_recursive(value, cases, 0);
}""")

with open(FP_HPP, "w") as f:
    f.write(fp_text)
