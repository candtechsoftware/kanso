template<typename T>
Vec2<T> operator+(const Vec2<T> a, const Vec2<T> b) {
    return Vec2<T>{ a.x + b.x, a.y + b.y };
}

template<typename T>
Vec2<T> operator-(const Vec2<T> a, const Vec2<T> b) {
    return Vec2<T>{ a.x - b.x, a.y - b.y };
}

template<typename T>
Vec2<T> operator*(const Vec2<T> a, const Vec2<T> b) {
    return Vec2<T>{ a.x * b.x, a.y * b.y };
}

template<typename T>
Vec2<T> operator/(const Vec2<T> a, const Vec2<T> b) {
    return Vec2<T>{ a.x / b.x, a.y / b.y };
}


template<typename T>
Vec2<T> operator/(const Vec2<T> a, T scalar) {
    return Vec2<T>{ a.x / scalar, a.y / scalar };
}



template<typename T>
bool operator==(const Vec2<T> a, const Vec2<T> b) {
    return a.x == b.x && a.y == b.y;
}
