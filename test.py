class Person:
    def __init__(self, name, age):
        self.name = name
        self.age = age

    def introduce(self):
        print(f"Tôi là {self.name}, {self.age} tuổi")

class Student(Person): # Kế thừa
    def __init__(self, name, age, student_id):
        super().__init__(name, age) # Gọi cha
        self.student_id = student_id

    def study(self):
        print(f"{self.name} đang học bài ")

sv = Student("Minh", 20, "SV001")
sv.introduce() # Kế thừa từ Person
sv.study() # Riêng của Student