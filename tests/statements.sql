CREATE TABLE Users (id int, name str, age int, city str);

INSERT INTO Users (id, name, age, city) VALUES (1, "John", 25, "New York"), (2, "Alice", 30, "London"), (3, "Bob", 22, "Paris");

SELECT * FROM Users;

INSERT INTO Users (id, name, age) VALUES (4, "Charlie", 28), (5, "Diana", 35);

SELECT * FROM Users;

UPDATE Users SET city = "Berlin" WHERE name = "Bob";

SELECT * FROM Users;

DELETE FROM Users WHERE age < 25;

SELECT * FROM Users;

INSERT INTO Users (id, name, age, city) VALUES (6, "Eve", 40, "Tokyo"), (7, "Frank", 29, "Sydney");

SELECT name, age FROM Users WHERE city != "";

UPDATE Users SET age = age + 1 WHERE city = "London";

SELECT * FROM Users;

DELETE FROM Users WHERE id = 4;

SELECT id, name, city FROM Users;

CREATE TABLE Products (product_id int, product_name str, price int, category str);

INSERT INTO Products (product_id, product_name, price, category) VALUES (1, "Laptop", 1000, "Electronics"), (2, "Book", 20, "Education"), (3, "Chair", 150, "Furniture");

SELECT * FROM Products;

UPDATE Products SET price = 1200 WHERE product_name = "Laptop";

SELECT product_name, price FROM Products WHERE category = "Electronics";

INSERT INTO Products (product_id, product_name, price) VALUES (4, "Pen", 2), (5, "Table", 300);

SELECT * FROM Products;

DELETE FROM Products WHERE price < 10;

SELECT * FROM Products;

SELECT product_name, category FROM Products WHERE price > 100;

UPDATE Products SET category = "Office" WHERE category = "";

SELECT * FROM Products;

SELECT * FROM Users WHERE age > 25 AND city != "";

DELETE FROM Users WHERE city = "";

SELECT * FROM Users;

SELECT name, city, age FROM Users ORDER BY age DESC;